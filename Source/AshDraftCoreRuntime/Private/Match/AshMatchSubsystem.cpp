// Copyright YoosungHong. All Rights Reserved.

#include "Match/AshMatchSubsystem.h"

#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Character/AshHeroCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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

	// React to base flips instead of polling (ARCHITECTURE.md 18.3).
	if (UAshBaseSubsystem* BaseSubsystem = InWorld.GetSubsystem<UAshBaseSubsystem>())
	{
		BaseSubsystem->OnAnyBaseOwnershipChanged.AddDynamic(this, &UAshMatchSubsystem::HandleBaseOwnershipChanged);
		bBoundToBases = true;
	}

	// Bind to the hero's death event; if the experience hasn't spawned it yet, retry on a timer.
	TryBindToHero();

	// All actors have finished BeginPlay, so bases are registered: begin the match.
	StartMatch();
}

void UAshMatchSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HeroBindTimerHandle);

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
		if (Base->IsMainBase() && Owner == EAshTeamId::Enemy)
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
	if (bAllPlayerSide)
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
	EndMatch(EAshMatchResult::Defeat, EAshMatchEndReason::PlayerDeath);
}
