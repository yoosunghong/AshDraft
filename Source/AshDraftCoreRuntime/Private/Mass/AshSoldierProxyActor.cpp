// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshSoldierProxyActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

AAshSoldierProxyActor::AAshSoldierProxyActor()
{
	// A proxy is a passive view driven by the representation processor; never ticks itself
	// (ARCHITECTURE.md 18.3). The skeletal mesh still evaluates its own pose, but only while
	// rendered (set below) so off-screen proxies stay cheap.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCanEverAffectNavigation(false);
	// Don't burn animation cost on proxies the player can't see (ARCHITECTURE.md 13/18).
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	// Idle until the pool assigns this proxy to an entity.
	SetActorHiddenInGame(true);
}

void AAshSoldierProxyActor::AssignToEntity(FMassEntityHandle Entity, EAshTeamId Team)
{
	RepresentedEntity = Entity;
	RepresentedTeam = Team;
	SetActorHiddenInGame(false);
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
}

void AAshSoldierProxyActor::SyncFromEntity(const FVector& Position, const FVector& Velocity, float /*HealthFraction*/)
{
	// Mass is the authority; the proxy simply follows the entity's position and turns to face
	// its movement so locomotion animation reads correctly. Health fraction is accepted for
	// future visual feedback (damage tint, scale) but unused in the PoC.
	SetActorLocation(Position);

	const FVector Planar(Velocity.X, Velocity.Y, 0.f);
	if (Planar.SizeSquared() > FMath::Square(FacingSpeedThreshold))
	{
		SetActorRotation(Planar.Rotation());
	}
}

FVector AAshSoldierProxyActor::GetSyncedLocation() const
{
	return GetActorLocation();
}

void AAshSoldierProxyActor::PlayAttackMontage()
{
	if (AttackMontage && MeshComponent)
	{
		if (UAnimInstance* Anim = MeshComponent->GetAnimInstance())
		{
			Anim->Montage_Play(AttackMontage);
		}
	}
}

void AAshSoldierProxyActor::PlayHitReactMontage()
{
	if (HitReactMontage && MeshComponent)
	{
		if (UAnimInstance* Anim = MeshComponent->GetAnimInstance())
		{
			Anim->Montage_Play(HitReactMontage);
		}
	}
}
