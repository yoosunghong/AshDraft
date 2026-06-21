// Copyright YoosungHong. All Rights Reserved.

#include "Base/AshBaseActor.h"

#include "Base/AshBaseConfig.h"
#include "Base/AshBaseSubsystem.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Teams/AshTeamStatics.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAshBase, Log, All);

namespace
{
	/** Two AshDraft teams share a side when both are Player/Ally; Enemy is the lone other side. */
	bool IsFriendlySide(EAshTeamId Team)
	{
		return Team == EAshTeamId::Player || Team == EAshTeamId::Ally;
	}
}

AAshBaseActor::AAshBaseActor()
{
	// No Actor Tick: the capture loop runs on a repeating timer (ARCHITECTURE.md 18.3).
	PrimaryActorTick.bCanEverTick = false;

	CaptureVolume = CreateDefaultSubobject<USphereComponent>(TEXT("CaptureVolume"));
	SetRootComponent(CaptureVolume);
	CaptureVolume->InitSphereRadius(FallbackCaptureRadius);
	CaptureVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CaptureVolume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CaptureVolume->SetGenerateOverlapEvents(true);
}

void AAshBaseActor::BeginPlay()
{
	Super::BeginPlay();

	CaptureVolume->SetSphereRadius(ResolveCaptureRadius());
	CurrentDurability = ResolveMaxDurability();

	if (UWorld* World = GetWorld())
	{
		if (UAshBaseSubsystem* Subsystem = World->GetSubsystem<UAshBaseSubsystem>())
		{
			Subsystem->RegisterBase(this);
		}

		const float Interval = Config ? Config->UpdateInterval : 0.5f;
		World->GetTimerManager().SetTimer(CaptureTimerHandle, this, &AAshBaseActor::UpdateCaptureState, Interval, true);
	}

	OnDurabilityChanged.Broadcast(this, CurrentDurability, ResolveMaxDurability());
}

void AAshBaseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CaptureTimerHandle);
		if (UAshBaseSubsystem* Subsystem = World->GetSubsystem<UAshBaseSubsystem>())
		{
			Subsystem->UnregisterBase(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

float AAshBaseActor::GetDurabilityNormalized() const
{
	const float Max = ResolveMaxDurability();
	return Max > 0.f ? CurrentDurability / Max : 0.f;
}

float AAshBaseActor::ResolveMaxDurability() const
{
	return Config ? Config->MaxDurability : FallbackMaxDurability;
}

float AAshBaseActor::ResolveCaptureRadius() const
{
	return Config ? Config->CaptureRadius : FallbackCaptureRadius;
}

void AAshBaseActor::RecountOccupants()
{
	TArray<AActor*> Overlapping;
	CaptureVolume->GetOverlappingActors(Overlapping, APawn::StaticClass());

	int32 PlayerCount = 0;
	int32 AllyCount = 0;
	int32 EnemyCount = 0;
	for (const AActor* Actor : Overlapping)
	{
		switch (UAshTeamStatics::GetActorTeam(Actor))
		{
		case EAshTeamId::Player: ++PlayerCount; break;
		case EAshTeamId::Ally:   ++AllyCount;   break;
		case EAshTeamId::Enemy:  ++EnemyCount;  break;
		default: break; // Neutral pawns do not capture.
		}
	}

	const int32 FriendlySide = PlayerCount + AllyCount;
	const int32 EnemySide = EnemyCount;

	// Classify the two sides relative to the current owner: who defends, who attacks,
	// and which specific team would take ownership if the attackers win.
	if (OwningTeam == EAshTeamId::Enemy)
	{
		DefenderCount = EnemySide;
		AttackerCount = FriendlySide;
		CapturingTeam = (PlayerCount >= AllyCount) ? EAshTeamId::Player : EAshTeamId::Ally;
	}
	else if (IsFriendlySide(OwningTeam))
	{
		DefenderCount = FriendlySide;
		AttackerCount = EnemySide;
		CapturingTeam = EAshTeamId::Enemy;
	}
	else // Neutral: the larger side captures; the smaller opposing presence contests.
	{
		if (FriendlySide >= EnemySide)
		{
			AttackerCount = FriendlySide;
			DefenderCount = EnemySide;
			CapturingTeam = (PlayerCount >= AllyCount) ? EAshTeamId::Player : EAshTeamId::Ally;
		}
		else
		{
			AttackerCount = EnemySide;
			DefenderCount = FriendlySide;
			CapturingTeam = EAshTeamId::Enemy;
		}
	}
}

void AAshBaseActor::UpdateCaptureState()
{
	RecountOccupants();

	const float Dt = Config ? Config->UpdateInterval : 0.5f;
	const bool bNewContested = (DefenderCount > 0 && AttackerCount > 0);
	if (bNewContested != bContested)
	{
		bContested = bNewContested;
		OnContestedChanged.Broadcast(this, bContested);
	}

	if (AttackerCount > 0 && !bContested)
	{
		// Uncontested attackers drain durability toward a capture.
		TimeSinceContested = 0.f;
		const float Rate = Config ? Config->CaptureRatePerSecond : 25.f;
		ChangeDurability(-Rate * Dt);
	}
	else if (AttackerCount == 0)
	{
		// No attackers: an owned base recovers after the quiet delay. Neutral bases do not.
		TimeSinceContested += Dt;
		const float Delay = Config ? Config->RecoveryDelay : 3.f;
		const float Max = ResolveMaxDurability();
		if (OwningTeam != EAshTeamId::Neutral && TimeSinceContested >= Delay && CurrentDurability < Max)
		{
			const float Rate = Config ? Config->RecoveryRatePerSecond : 15.f;
			ChangeDurability(Rate * Dt);
		}
	}
	else
	{
		// Contested: frozen.
		TimeSinceContested = 0.f;
	}

	if (bShowDebug)
	{
		DrawDebug();
	}
}

void AAshBaseActor::ChangeDurability(float Delta)
{
	const float Max = ResolveMaxDurability();
	const float Old = CurrentDurability;
	CurrentDurability = FMath::Clamp(CurrentDurability + Delta, 0.f, Max);

	if (!FMath::IsNearlyEqual(Old, CurrentDurability))
	{
		OnDurabilityChanged.Broadcast(this, CurrentDurability, Max);
		K2_OnDurabilityChanged(CurrentDurability, Max);
	}

	// Durability spent: ownership flips to the team that drained it.
	if (CurrentDurability <= 0.f && CapturingTeam != EAshTeamId::Neutral && CapturingTeam != OwningTeam)
	{
		SetOwningTeam(CapturingTeam);
	}
}

void AAshBaseActor::SetOwningTeam(EAshTeamId NewTeam)
{
	const EAshTeamId OldTeam = OwningTeam;
	if (OldTeam == NewTeam)
	{
		return;
	}

	OwningTeam = NewTeam;
	CurrentDurability = ResolveMaxDurability(); // new owner starts fully entrenched
	CapturingTeam = EAshTeamId::Neutral;
	TimeSinceContested = 0.f;

	UE_LOG(LogAshBase, Log, TEXT("Base '%s' captured: %d -> %d"), *GetName(), static_cast<int32>(OldTeam), static_cast<int32>(NewTeam));

	OnOwnershipChanged.Broadcast(this, OldTeam, NewTeam);
	K2_OnOwnershipChanged(OldTeam, NewTeam);
	OnDurabilityChanged.Broadcast(this, CurrentDurability, ResolveMaxDurability());

	if (UWorld* World = GetWorld())
	{
		if (UAshBaseSubsystem* Subsystem = World->GetSubsystem<UAshBaseSubsystem>())
		{
			Subsystem->NotifyOwnershipChanged(this, OldTeam, NewTeam);
		}
	}

	// DefenderRespawnDelay is a Phase 8 hook; soldiers spawn here once they exist.
}

void AAshBaseActor::NotifyDefenderDied(EAshTeamId DefenderTeam)
{
	if (DefenderTeam != OwningTeam || OwningTeam == EAshTeamId::Neutral)
	{
		return;
	}

	// Refresh CapturingTeam so a death during an assault can trigger a flip at zero.
	RecountOccupants();
	const float Amount = Config ? Config->DurabilityPerDefenderDeath : 10.f;
	ChangeDurability(-Amount);
}

void AAshBaseActor::ApplyCaptureDamage(EAshTeamId FromTeam, float Amount)
{
	if (Amount <= 0.f || !UAshTeamStatics::AreTeamsHostile(FromTeam, OwningTeam))
	{
		// Neutral owner is hostile to no one; still allow a present team to claim it.
		if (!(OwningTeam == EAshTeamId::Neutral && FromTeam != EAshTeamId::Neutral && Amount > 0.f))
		{
			return;
		}
	}

	CapturingTeam = FromTeam;
	ChangeDurability(-Amount);
}

void AAshBaseActor::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor RingColor = bContested ? FColor::Yellow : (AttackerCount > 0 ? FColor::Red : FColor::Green);
	DrawDebugSphere(World, GetActorLocation(), ResolveCaptureRadius(), 24, RingColor, false, Config ? Config->UpdateInterval : 0.5f, 0, 3.f);

	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("Base '%s'  Owner=%d  Dur=%.0f/%.0f  Def=%d Atk=%d  %s"),
			*GetName(), static_cast<int32>(OwningTeam), CurrentDurability, ResolveMaxDurability(),
			DefenderCount, AttackerCount, bContested ? TEXT("CONTESTED") : TEXT(""));
		GEngine->AddOnScreenDebugMessage(static_cast<int32>(GetUniqueID()), (Config ? Config->UpdateInterval : 0.5f) + 0.05f, RingColor, Msg);
	}
#endif // ENABLE_DRAW_DEBUG
}
