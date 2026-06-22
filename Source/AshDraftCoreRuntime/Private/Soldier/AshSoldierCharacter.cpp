// Copyright YoosungHong. All Rights Reserved.

#include "Soldier/AshSoldierCharacter.h"

#include "AIController.h"
#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Soldier/AshSoldierConfig.h"
#include "Soldier/AshSoldierSubsystem.h"
#include "TimerManager.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAshSoldier, Log, All);

AAshSoldierCharacter::AAshSoldierCharacter()
{
	// No per-frame Actor Tick (ARCHITECTURE.md 18.3); local rules run on a think timer.
	PrimaryActorTick.bCanEverTick = false;

	// A plain AIController gives us NavMesh path-following for the prototype's MoveTo
	// calls without a per-soldier Behavior Tree (ARCHITECTURE.md 7.4).
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, 540.f, 0.f);
		MoveComp->bConstrainToPlane = true;
		MoveComp->bSnapToPlaneAtStart = true;
	}
}

void AAshSoldierCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = ResolveMaxHealth();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = ResolveMoveSpeed();
	}

	// Default objective is to hold the spawn location until one is assigned.
	if (!bHasObjective)
	{
		ObjectiveLocation = GetActorLocation();
		bHasObjective = true;
	}

	if (UWorld* World = GetWorld())
	{
		if (UAshSoldierSubsystem* Subsystem = World->GetSubsystem<UAshSoldierSubsystem>())
		{
			Subsystem->RegisterSoldier(this);
		}

		// Stagger the first think by a tiny per-instance offset so a wave of soldiers
		// spawned on the same frame don't all path/attack in lockstep.
		const float Interval = ResolveThinkInterval();
		const float FirstDelay = Interval + FMath::FRandRange(0.f, Interval);
		World->GetTimerManager().SetTimer(ThinkTimerHandle, this, &AAshSoldierCharacter::Think, Interval, true, FirstDelay);
	}
}

void AAshSoldierCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimerHandle);
		if (UAshSoldierSubsystem* Subsystem = World->GetSubsystem<UAshSoldierSubsystem>())
		{
			Subsystem->UnregisterSoldier(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AAshSoldierCharacter::Think()
{
	if (bIsDead)
	{
		return;
	}

	AcquireTarget();

	if (AAshSoldierCharacter* Target = CurrentTarget.Get())
	{
		const float DistSq = FVector::DistSquared(GetActorLocation(), Target->GetActorLocation());
		const float Range = ResolveAttackRange();
		if (DistSq <= Range * Range)
		{
			// In reach: stop and strike (ARCHITECTURE.md 8.3 simple local rule).
			if (AAIController* AIController = Cast<AAIController>(GetController()))
			{
				AIController->StopMovement();
			}
			bHasActiveMove = false;
			TryAttack();
		}
		else
		{
			MoveToward(Target, Target->GetActorLocation());
		}
	}
	else if (bHasObjective)
	{
		// No enemy in range: advance toward the squad/objective location.
		MoveToward(nullptr, ObjectiveLocation);
	}

	if (bShowDebug)
	{
		DrawDebug();
	}
}

void AAshSoldierCharacter::AcquireTarget()
{
	AAshSoldierCharacter* Target = CurrentTarget.Get();
	const float Detection = ResolveDetectionRadius();

	// Drop a target that died or wandered out of detection range.
	if (Target)
	{
		const bool bStillValid = !Target->IsDead() &&
			FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= Detection * Detection;
		if (!bStillValid)
		{
			CurrentTarget = nullptr;
			Target = nullptr;
		}
	}

	if (!Target)
	{
		if (UWorld* World = GetWorld())
		{
			if (UAshSoldierSubsystem* Subsystem = World->GetSubsystem<UAshSoldierSubsystem>())
			{
				CurrentTarget = Subsystem->FindNearestHostileSoldier(TeamId, GetActorLocation(), Detection);
			}
		}
	}
}

void AAshSoldierCharacter::MoveToward(AActor* GoalActor, const FVector& GoalLocation)
{
	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		return;
	}

	const float Acceptance = ResolveAcceptanceRadius();

	// Debounce: only re-issue a move request when the goal has meaningfully changed,
	// so following a moving target doesn't spam the path-following component.
	if (bHasActiveMove)
	{
		if (GoalActor && ActiveMoveGoalActor.Get() == GoalActor)
		{
			return;
		}
		if (!GoalActor && !ActiveMoveGoalActor.IsValid() &&
			FVector::DistSquared(ActiveMoveGoalLocation, GoalLocation) <= FMath::Square(Acceptance))
		{
			return;
		}
	}

	if (GoalActor)
	{
		AIController->MoveToActor(GoalActor, Acceptance);
	}
	else
	{
		AIController->MoveToLocation(GoalLocation, Acceptance);
	}

	ActiveMoveGoalActor = GoalActor;
	ActiveMoveGoalLocation = GoalLocation;
	bHasActiveMove = true;
}

void AAshSoldierCharacter::TryAttack()
{
	AAshSoldierCharacter* Target = CurrentTarget.Get();
	if (!Target || Target->IsDead())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastAttackTime < ResolveAttackInterval())
	{
		return;
	}
	LastAttackTime = Now;

	// Face the target for readability (movement orientation is idle while attacking).
	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;
	if (!ToTarget.IsNearlyZero())
	{
		SetActorRotation(ToTarget.Rotation());
	}

	// Authority applies the damage so the model stays multiplayer-friendly (5.5 / 15).
	if (HasAuthority())
	{
		Target->ReceiveDamage(ResolveAttackDamage(), this);
	}
}

void AAshSoldierCharacter::ReceiveDamage(float Amount, AActor* InInstigator)
{
	if (bIsDead || Amount <= 0.f || !HasAuthority())
	{
		return;
	}

	CurrentHealth = FMath::Max(0.f, CurrentHealth - Amount);
	if (CurrentHealth <= 0.f)
	{
		HandleDeath(InInstigator);
	}
}

void AAshSoldierCharacter::HandleDeath(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// Stop the think loop and any in-flight movement.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimerHandle);
	}
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Drive the Phase 7 base capture loop: a defender death drains its base's durability
	// (ARCHITECTURE.md 11.2). Notify the nearest base that owns this soldier's team.
	if (UWorld* World = GetWorld())
	{
		if (UAshBaseSubsystem* BaseSubsystem = World->GetSubsystem<UAshBaseSubsystem>())
		{
			AAshBaseActor* NearestOwned = nullptr;
			float BestDistSq = TNumericLimits<float>::Max();
			for (AAshBaseActor* Base : BaseSubsystem->GetAllBases())
			{
				if (Base && Base->GetOwningTeam() == TeamId)
				{
					const float DistSq = FVector::DistSquared(GetActorLocation(), Base->GetActorLocation());
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						NearestOwned = Base;
					}
				}
			}
			if (NearestOwned)
			{
				NearestOwned->NotifyDefenderDied(TeamId);
			}
		}

		// Leave the registry immediately so it stops being targeted, even before despawn.
		if (UAshSoldierSubsystem* Subsystem = World->GetSubsystem<UAshSoldierSubsystem>())
		{
			Subsystem->UnregisterSoldier(this);
		}
	}

	UE_LOG(LogAshSoldier, Verbose, TEXT("Soldier '%s' (team %d) died"), *GetName(), static_cast<int32>(TeamId));

	OnSoldierDied.Broadcast(this, TeamId);

	const float LifeSpan = Config ? Config->DeathLifeSpan : 3.f;
	if (LifeSpan > 0.f)
	{
		SetLifeSpan(LifeSpan);
	}
}

void AAshSoldierCharacter::SetObjectiveLocation(const FVector& InLocation)
{
	ObjectiveLocation = InLocation;
	bHasObjective = true;
	// Invalidate any active move so the next think re-paths to the new objective.
	bHasActiveMove = false;
}

float AAshSoldierCharacter::GetMaxHealth() const
{
	return ResolveMaxHealth();
}

float AAshSoldierCharacter::ResolveMaxHealth() const       { return Config ? Config->MaxHealth : FallbackMaxHealth; }
float AAshSoldierCharacter::ResolveMoveSpeed() const        { return Config ? Config->MoveSpeed : FallbackMoveSpeed; }
float AAshSoldierCharacter::ResolveAttackRange() const      { return Config ? Config->AttackRange : FallbackAttackRange; }
float AAshSoldierCharacter::ResolveAttackDamage() const     { return Config ? Config->AttackDamage : FallbackAttackDamage; }
float AAshSoldierCharacter::ResolveAttackInterval() const   { return Config ? Config->AttackInterval : FallbackAttackInterval; }
float AAshSoldierCharacter::ResolveDetectionRadius() const  { return Config ? Config->DetectionRadius : FallbackDetectionRadius; }
float AAshSoldierCharacter::ResolveThinkInterval() const    { return Config ? Config->ThinkInterval : FallbackThinkInterval; }
float AAshSoldierCharacter::ResolveAcceptanceRadius() const { return Config ? Config->MoveAcceptanceRadius : FallbackAcceptanceRadius; }

void AAshSoldierCharacter::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor TeamColor = (TeamId == EAshTeamId::Enemy) ? FColor::Red : FColor::Cyan;
	const FString Msg = FString::Printf(TEXT("HP %.0f/%.0f%s"),
		CurrentHealth, ResolveMaxHealth(), CurrentTarget.IsValid() ? TEXT("  [engaging]") : TEXT(""));
	DrawDebugString(World, FVector(0.f, 0.f, 100.f), Msg, const_cast<AAshSoldierCharacter*>(this), TeamColor, ResolveThinkInterval() + 0.05f);
#endif // ENABLE_DRAW_DEBUG
}
