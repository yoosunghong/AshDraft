// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshSoldierProxyActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Mass/AshSoldierAnimInstance.h"
#include "Mass/AshSoldierFragments.h"
#include "Mass/AshSoldierVisualConfig.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

AAshSoldierProxyActor::AAshSoldierProxyActor()
{
	// A proxy is a passive view driven by the representation processor; never ticks itself
	// (ARCHITECTURE.md 18.3). The skeletal mesh still evaluates its own pose, but only while
	// rendered (set below) so off-screen proxies stay cheap.
	PrimaryActorTick.bCanEverTick = false;

	// Plain scene root carries the entity's *facing*; the mesh hangs under it with its own
	// art-correction transform (set by ConfigureVisual). Keeping these on separate components is
	// what stops SyncFromEntity's SetActorRotation from clobbering the mesh's yaw offset.
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootScene);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCanEverAffectNavigation(false);
	// Don't burn animation cost on proxies the player can't see (ARCHITECTURE.md 13/18).
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	// Query-only hit capsule: the target for player/general melee sweeps (Phase 21). It overlaps the
	// Pawn channel only, so attack sweeps (SweepMultiByChannel on ECC_Pawn) find it, but it never
	// blocks movement, never simulates physics, and never pushes other soldiers (spacing stays the
	// data-oriented steering separation — adding physics push here would re-introduce crowd shaking).
	HitCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitCapsule"));
	HitCapsule->SetupAttachment(RootScene);
	HitCapsule->SetCapsuleSize(34.f, 90.f);
	HitCapsule->SetRelativeLocation(FVector(0.f, 0.f, 90.f)); // feet on the entity position
	HitCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); // enabled on assignment
	HitCapsule->SetCollisionObjectType(ECC_Pawn);
	HitCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitCapsule->SetGenerateOverlapEvents(false); // queried by sweeps, not an event source
	HitCapsule->SetCanEverAffectNavigation(false);

	// Idle until the pool assigns this proxy to an entity.
	SetActorHiddenInGame(true);
}

void AAshSoldierProxyActor::AssignToEntity(FMassEntityHandle Entity, EAshTeamId Team)
{
	RepresentedEntity = Entity;
	RepresentedTeam = Team;
	SetActorHiddenInGame(false);

	// Become a hittable target only while representing a live soldier (pooled proxies must not be
	// stray hits under the player's attack sweep).
	if (HitCapsule)
	{
		HitCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void AAshSoldierProxyActor::ClearAssignment()
{
	// Stop any montage still playing so a recycled proxy starts clean on its next entity.
	if (MeshComponent)
	{
		if (UAnimInstance* Anim = MeshComponent->GetAnimInstance())
		{
			Anim->Montage_Stop(0.f);
		}
	}

	RepresentedEntity.Reset();
	RepresentedTeam = EAshTeamId::Neutral;
	SetActorHiddenInGame(true);

	// Parked proxies are not hittable.
	if (HitCapsule)
	{
		HitCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AAshSoldierProxyActor::SyncFromEntity(const FVector& Position, float FacingYaw, const FVector& Velocity, float /*HealthFraction*/)
{
	// Mass is the authority; the proxy follows the entity's position and faces the Mass-authoritative
	// heading. Health fraction is accepted for future visual feedback (damage tint, scale) but unused
	// in the PoC.
	SetActorLocation(Position);

	// Facing comes from Mass (target-aware + interpolated), NOT from raw velocity — so a soldier that
	// has stopped in melee still faces its enemy instead of keeping a stale travel heading (Phase 20).
	// Rotates the scene root only; the mesh keeps its art-correction yaw (see RootScene).
	SetActorRotation(FRotator(0.f, FacingYaw, 0.f));

	const FVector Planar(Velocity.X, Velocity.Y, 0.f);
	const float PlanarSpeed = Planar.Size();
	const bool bMoving = PlanarSpeed > FacingSpeedThreshold;

	// Feed Mass-authoritative locomotion to the AnimBP. The proxy has no movement component, so the
	// AnimBP can't read GetVelocity(); this is how Idle<->Move blends get a real speed. Only rendered
	// proxies have a live anim instance (OnlyTickPoseWhenRendered), so a null here is expected/cheap.
	if (UAshSoldierAnimInstance* SoldierAnim = Cast<UAshSoldierAnimInstance>(MeshComponent->GetAnimInstance()))
	{
		SoldierAnim->GroundSpeed = PlanarSpeed;
		SoldierAnim->bIsMoving = bMoving;
	}
}

FVector AAshSoldierProxyActor::GetSyncedLocation() const
{
	return GetActorLocation();
}

void AAshSoldierProxyActor::ConfigureVisual(const UAshSoldierVisualConfig* Visual)
{
	// Pooled proxies are reused across entities (and unit types); only re-dress when the visual
	// set actually changes. This is called every frame for promoted entities, so the early-out
	// keeps the common case free.
	if (Visual == CurrentVisual || !MeshComponent)
	{
		return;
	}
	CurrentVisual = Visual;

	if (!Visual)
	{
		return; // No data: keep whatever (if anything) the Blueprint authored.
	}

	MeshComponent->SetSkeletalMeshAsset(Visual->SkeletalMesh);
	MeshComponent->SetRelativeLocation(Visual->MeshRelativeLocation);
	MeshComponent->SetRelativeRotation(Visual->MeshRelativeRotation);
	MeshComponent->SetRelativeScale3D(FVector(Visual->MeshScale));

	// Anim class is applied after the mesh so the new AnimInstance binds to the right skeleton.
	if (Visual->AnimClass)
	{
		MeshComponent->SetAnimInstanceClass(Visual->AnimClass);
	}

	// Fit the hit capsule to this unit type's body so the melee sweep tests a sensible volume, and
	// keep its feet on the entity position (capsule centre at +half-height).
	if (HitCapsule)
	{
		HitCapsule->SetCapsuleSize(Visual->CapsuleRadius, Visual->CapsuleHalfHeight);
		HitCapsule->SetRelativeLocation(FVector(0.f, 0.f, Visual->CapsuleHalfHeight));
	}
}

void AAshSoldierProxyActor::ReceiveMeleeHit(float Damage, const AActor* /*Instigator*/)
{
	// Mass is authoritative: route the actor-space hit back into the entity's health fragment. The
	// proxy is only the visible body / hit target; the combat + death processors do the rest.
	if (!RepresentedEntity.IsSet() || Damage <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(RepresentedEntity))
	{
		return;
	}

	if (FAshHealthFragment* Health = EntityManager.GetFragmentDataPtr<FAshHealthFragment>(RepresentedEntity))
	{
		Health->CurrentHealth = FMath::Max(0.f, Health->CurrentHealth - Damage);

		// Raise the one-shot hit-react event so the representation processor plays the flinch montage
		// this frame (same path the Mass combat processor uses).
		if (FAshCombatEventFragment* Event = EntityManager.GetFragmentDataPtr<FAshCombatEventFragment>(RepresentedEntity))
		{
			Event->bWasHitThisTick = true;
		}
	}
}

void AAshSoldierProxyActor::PlayAttackMontage()
{
	PlayMontage(CurrentVisual ? CurrentVisual->AttackMontage : nullptr);
}

void AAshSoldierProxyActor::PlayHitReactMontage()
{
	PlayMontage(CurrentVisual ? CurrentVisual->HitReactMontage : nullptr);
}

void AAshSoldierProxyActor::PlayMontage(UAnimMontage* Montage)
{
	if (!Montage || !MeshComponent)
	{
		return;
	}
	if (UAnimInstance* Anim = MeshComponent->GetAnimInstance())
	{
		Anim->Montage_Play(Montage);
	}
}
