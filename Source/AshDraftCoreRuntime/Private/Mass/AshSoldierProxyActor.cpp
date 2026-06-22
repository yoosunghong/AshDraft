// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshSoldierProxyActor.h"

#include "Components/StaticMeshComponent.h"

AAshSoldierProxyActor::AAshSoldierProxyActor()
{
	// A proxy is a passive view driven by the representation processor; never ticks itself
	// (ARCHITECTURE.md 18.3).
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCanEverAffectNavigation(false);

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
	RepresentedEntity.Reset();
	RepresentedTeam = EAshTeamId::Neutral;
	SetActorHiddenInGame(true);
}

void AAshSoldierProxyActor::SyncFromEntity(const FVector& Position, float /*HealthFraction*/)
{
	// Mass is the authority; the proxy simply follows the entity's position. Health fraction
	// is accepted for future visual feedback (damage tint, scale) but unused in the PoC.
	SetActorLocation(Position);
}

FVector AAshSoldierProxyActor::GetSyncedLocation() const
{
	return GetActorLocation();
}
