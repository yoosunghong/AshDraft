// Copyright YoosungHong. All Rights Reserved.

#include "Match/AshMatchSubsystem.h"

#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Character/AshEnemyGeneralCharacter.h"
#include "Character/AshHeroCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Match/AshMatchRulesActor.h"
#include "Match/AshMatchRulesConfig.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshMatch, Log, All);

namespace
{
	/** Retry cadence (s) for binding to the hero, which the experience may spawn after BeginPlay. */
	constexpr float HeroBindRetryInterval = 0.5f;

	/** On-screen result banner duration (s) — the UMG-free verification fallback. */
	constexpr float ResultBannerSeconds = 8.f;
}

bool UAshMatchSubsystem::IsPlayerSide(EAshTeamId Team)
{
	return Team == EAshTeamId::Player || Team == EAshTeamId::Ally;
}

void UAshMatchSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Adopt the level's editor-authored win/lose conditions (or keep the defaults).
	ResolveRules();

	// React to base flips instead of polling (ARCHITECTURE.md 18.3).
	if (UAshBaseSubsystem* BaseSubsystem = InWorld.GetSubsystem<UAshBaseSubsystem>())
	{
		BaseSubsystem->OnAnyBaseOwnershipChanged.AddDynamic(this, &UAshMatchSubsystem::HandleBaseOwnershipChanged);
		bBoundToBases = true;
	}

	// Bind to the hero's death event; if the experience hasn't spawned it yet, retry on a timer.
	TryBindToHero();

	// Bind enemy generals for the eliminate-generals victory (counts the living ones).
	BindToEnemyGenerals();

	// All actors have finished BeginPlay, so bases are registered: begin the match.
	StartMatch();
}

void UAshMatchSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HeroBindTimerHandle);
		World->GetTimerManager().ClearTimer(TimeLimitTimerHandle);

		if (bBoundToBases)
		{
			if (UAshBaseSubsystem* BaseSubsystem = World->GetSubsystem<UAshBaseSubsystem>())
			{
				BaseSubsystem->OnAnyBaseOwnershipChanged.RemoveDynamic(this, &UAshMatchSubsystem::HandleBaseOwnershipChanged);
			}
		}
	}
	bBoundToBases = false;

	if (AAshHeroCharacter* Hero = BoundHero.Get())
	{
		Hero->OnHeroDied.RemoveDynamic(this, &UAshMatchSubsystem::HandleHeroDied);
	}
	BoundHero = nullptr;

	for (const TWeakObjectPtr<AAshEnemyGeneralCharacter>& General : BoundGenerals)
	{
		if (AAshEnemyGeneralCharacter* Live = General.Get())
		{
			Live->OnGeneralDied.RemoveDynamic(this, &UAshMatchSubsystem::HandleGeneralDied);
		}
	}
	BoundGenerals.Reset();

	Super::Deinitialize();
}

void UAshMatchSubsystem::StartMatch()
{
	if (MatchState != EAshMatchState::NotStarted)
	{
		return;
	}

	const UWorld* World = GetWorld();
	MatchStartTime = World ? World->GetTimeSeconds() : 0.f;

	SetMatchState(EAshMatchState::InProgress);
	UE_LOG(LogAshMatch, Log, TEXT("Match started."));
	OnMatchStarted.Broadcast();

	// Start the optional time limit (Phase 15 rules). Outcome None / non-positive time = no timer.
	if (Rules && Rules->TimeLimitOutcome != EAshTimeLimitOutcome::None && Rules->TimeLimitSeconds > 0.f)
	{
		if (UWorld* TimerWorld = GetWorld())
		{
			TimerWorld->GetTimerManager().SetTimer(TimeLimitTimerHandle, this,
				&UAshMatchSubsystem::HandleTimeLimitReached, Rules->TimeLimitSeconds, false);
			UE_LOG(LogAshMatch, Log, TEXT("Match time limit armed: %.1fs (outcome %d)."),
				Rules->TimeLimitSeconds, static_cast<int32>(Rules->TimeLimitOutcome));
		}
	}

	// A starting layout could already satisfy a base condition (e.g. a one-base test map).
	EvaluateBaseConditions();
}

void UAshMatchSubsystem::EndMatch(EAshMatchResult Result, EAshMatchEndReason Reason)
{
	// Only end a live match, and only once (terminal states are sticky).
	if (MatchState != EAshMatchState::InProgress)
	{
		return;
	}

	MatchResult = Result;
	EndReason = Reason;

	const UWorld* World = GetWorld();
	MatchEndTime = World ? World->GetTimeSeconds() : MatchStartTime;

	SetMatchState(Result == EAshMatchResult::Victory ? EAshMatchState::Victory : EAshMatchState::Defeat);

	UE_LOG(LogAshMatch, Log, TEXT("Match ended: Result=%d Reason=%d (elapsed %.1fs)"),
		static_cast<int32>(Result), static_cast<int32>(Reason), GetMatchElapsedSeconds());

	OnMatchEnded.Broadcast(Result, Reason);

	// UMG-free verification fallback: show the result on screen (ARCHITECTURE.md 16 leaves the
	// real widget to UI; this proves the loop terminates without it).
	if (GEngine)
	{
		const FString Banner = (Result == EAshMatchResult::Victory) ? TEXT("VICTORY") : TEXT("DEFEAT");
		const FColor Color = (Result == EAshMatchResult::Victory) ? FColor::Green : FColor::Red;
		GEngine->AddOnScreenDebugMessage(-1, ResultBannerSeconds, Color,
			FString::Printf(TEXT("%s  (reason %d)"), *Banner, static_cast<int32>(Reason)));
	}
}

float UAshMatchSubsystem::GetMatchElapsedSeconds() const
{
	if (MatchState == EAshMatchState::NotStarted)
	{
		return 0.f;
	}
	if (MatchState == EAshMatchState::InProgress)
	{
		const UWorld* World = GetWorld();
		return (World ? World->GetTimeSeconds() : MatchStartTime) - MatchStartTime;
	}
	return MatchEndTime - MatchStartTime;
}

void UAshMatchSubsystem::SetMatchState(EAshMatchState NewState)
{
	if (MatchState == NewState)
	{
		return;
	}
	MatchState = NewState;
	OnMatchStateChanged.Broadcast(NewState);
}

void UAshMatchSubsystem::HandleBaseOwnershipChanged(AAshBaseActor* /*Base*/, EAshTeamId /*OldTeam*/, EAshTeamId /*NewTeam*/)
{
	EvaluateBaseConditions();
}

void UAshMatchSubsystem::EvaluateBaseConditions()
{
	if (MatchState != EAshMatchState::InProgress)
	{
		return;
	}

	const UWorld* World = GetWorld();
	UAshBaseSubsystem* BaseSubsystem = World ? World->GetSubsystem<UAshBaseSubsystem>() : nullptr;
	if (!BaseSubsystem)
	{
		return;
	}

	const bool bCheckMainBaseLost = Rule_DefeatWhenMainBaseLost();
	const bool bCheckAllBases = Rule_VictoryWhenAllBasesCaptured();
	if (!bCheckMainBaseLost && !bCheckAllBases)
	{
		return; // No base-driven conditions are active under the current rules.
	}

	const TArray<AAshBaseActor*> Bases = BaseSubsystem->GetAllBases();
	if (Bases.Num() == 0)
	{
		return; // No bases to win or lose with.
	}

	bool bAllPlayerSide = true;
	for (const AAshBaseActor* Base : Bases)
	{
		if (!Base)
		{
			continue;
		}

		const EAshTeamId Owner = Base->GetOwningTeam();

		// Defeat: a player-side main base has fallen to the enemy.
		if (bCheckMainBaseLost && Base->IsMainBase() && Owner == EAshTeamId::Enemy)
		{
			EndMatch(EAshMatchResult::Defeat, EAshMatchEndReason::MainBaseLost);
			return;
		}

		if (!IsPlayerSide(Owner))
		{
			bAllPlayerSide = false;
		}
	}

	// Victory: the player side owns every base.
	if (bCheckAllBases && bAllPlayerSide)
	{
		EndMatch(EAshMatchResult::Victory, EAshMatchEndReason::AllBasesCaptured);
	}
}

void UAshMatchSubsystem::TryBindToHero()
{
	if (BoundHero.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AAshHeroCharacter* Hero = nullptr;
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		Hero = Cast<AAshHeroCharacter>(PC->GetPawn());
	}

	if (Hero)
	{
		Hero->OnHeroDied.AddDynamic(this, &UAshMatchSubsystem::HandleHeroDied);
		BoundHero = Hero;
		World->GetTimerManager().ClearTimer(HeroBindTimerHandle);
		UE_LOG(LogAshMatch, Verbose, TEXT("Bound to hero '%s' for player-death defeat."), *Hero->GetName());
	}
	else
	{
		// Hero not spawned yet (experience pipeline): retry shortly. Harmless once terminal.
		World->GetTimerManager().SetTimer(HeroBindTimerHandle, this, &UAshMatchSubsystem::TryBindToHero, HeroBindRetryInterval, false);
	}
}

void UAshMatchSubsystem::HandleHeroDied(AAshHeroCharacter* /*Hero*/)
{
	if (!Rule_DefeatWhenHeroDies())
	{
		return;
	}
	EndMatch(EAshMatchResult::Defeat, EAshMatchEndReason::PlayerDeath);
}

void UAshMatchSubsystem::HandleGeneralDied(AAshEnemyGeneralCharacter* /*General*/)
{
	RemainingEnemyGenerals = FMath::Max(0, RemainingEnemyGenerals - 1);

	if (Rule_VictoryWhenEnemyGeneralsEliminated() && RemainingEnemyGenerals == 0)
	{
		EndMatch(EAshMatchResult::Victory, EAshMatchEndReason::EnemyGeneralsEliminated);
	}
}

void UAshMatchSubsystem::HandleTimeLimitReached()
{
	if (!Rules)
	{
		return;
	}

	switch (Rules->TimeLimitOutcome)
	{
	case EAshTimeLimitOutcome::Victory:
		EndMatch(EAshMatchResult::Victory, EAshMatchEndReason::TimeLimitReached);
		break;
	case EAshTimeLimitOutcome::Defeat:
		EndMatch(EAshMatchResult::Defeat, EAshMatchEndReason::TimeLimitReached);
		break;
	default:
		break;
	}
}

void UAshMatchSubsystem::ResolveRules()
{
	Rules = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// First rules actor wins; the level is expected to hold at most one.
	for (TActorIterator<AAshMatchRulesActor> It(World); It; ++It)
	{
		if (const UAshMatchRulesConfig* Found = It->GetRules())
		{
			Rules = Found;
			UE_LOG(LogAshMatch, Log, TEXT("Match rules adopted from '%s' (asset '%s')."),
				*It->GetName(), *Found->GetName());
			return;
		}
	}

	UE_LOG(LogAshMatch, Log, TEXT("No match rules actor/asset found; using Phase 16 defaults."));
}

void UAshMatchSubsystem::BindToEnemyGenerals()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AAshEnemyGeneralCharacter> It(World); It; ++It)
	{
		AAshEnemyGeneralCharacter* General = *It;
		if (!General || General->IsDead())
		{
			continue;
		}

		General->OnGeneralDied.AddDynamic(this, &UAshMatchSubsystem::HandleGeneralDied);
		BoundGenerals.Add(General);
		++RemainingEnemyGenerals;
	}

	UE_LOG(LogAshMatch, Verbose, TEXT("Bound to %d enemy general(s) for the eliminate-generals victory."),
		RemainingEnemyGenerals);
}

bool UAshMatchSubsystem::Rule_VictoryWhenAllBasesCaptured() const
{
	return Rules ? Rules->bVictoryWhenAllBasesCaptured : true;
}

bool UAshMatchSubsystem::Rule_VictoryWhenEnemyGeneralsEliminated() const
{
	// Only meaningful when the level actually has generals to eliminate.
	return Rules ? (Rules->bVictoryWhenEnemyGeneralsEliminated && BoundGenerals.Num() > 0) : false;
}

bool UAshMatchSubsystem::Rule_DefeatWhenHeroDies() const
{
	return Rules ? Rules->bDefeatWhenHeroDies : true;
}

bool UAshMatchSubsystem::Rule_DefeatWhenMainBaseLost() const
{
	return Rules ? Rules->bDefeatWhenMainBaseLost : true;
}
