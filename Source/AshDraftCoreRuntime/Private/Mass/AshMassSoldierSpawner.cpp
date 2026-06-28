// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassSoldierSpawner.h"

#include "AI/AshCommanderSubsystem.h"
#include "AI/AshSquadSubsystem.h"
#include "Engine/World.h"
#include "Mass/AshMassSoldierConfig.h"
#include "Mass/AshMassSoldierSpawnLibrary.h"
#include "Mass/AshSoldierFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAshMassSoldier, Log, All);

AAshMassSoldierSpawner::AAshMassSoldierSpawner()
{
	// Pure data spawner: entities are simulated by Mass Processors (Phase 10), never by
	// Actor Tick (ARCHITECTURE.md 18.3).
	PrimaryActorTick.bCanEverTick = false;
}

void AAshMassSoldierSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnSoldiers();
	}
}

void AAshMassSoldierSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DespawnSoldiers();
	Super::EndPlay(EndPlayReason);
}

int32 AAshMassSoldierSpawner::Respawn()
{
	DespawnSoldiers();
	const int32 NewCount = SpawnSoldiers();
	UE_LOG(LogAshMassSoldier, Log, TEXT("AAshMassSoldierSpawner '%s': respawned %d soldiers (Team=%d, Squad=%d)."),
		*GetName(), NewCount, static_cast<int32>(TeamId), SquadId);
	return NewCount;
}

int32 AAshMassSoldierSpawner::SpawnSoldiers()
{
	if (SpawnCount <= 0)
	{
		return 0;
	}

	UWorld* World = GetWorld();

	// Delegate the archetype build + fragment seeding to the shared library so the spawner and the
	// Phase 22 General create soldiers through one code path (CLAUDE.md DRY).
	AshMassSoldierSpawn::FAshSoldierSpawnParams Params;
	Params.Config = Config;
	Params.TeamId = TeamId;
	Params.SquadId = SquadId;
	Params.Count = SpawnCount;
	Params.Origin = GetActorLocation();
	Params.SpawnRadius = SpawnRadius;
	Params.FallbackMaxHealth = FallbackMaxHealth;
	Params.FallbackMoveSpeed = FallbackMoveSpeed;
	Params.FallbackAttackRange = FallbackAttackRange;
	Params.FallbackAttackPower = FallbackAttackPower;
	Params.FallbackAttackCooldown = FallbackAttackCooldown;

	const int32 NumCreated = AshMassSoldierSpawn::SpawnSoldiers(World, Params, SpawnedEntities);
	if (NumCreated == 0)
	{
		return 0;
	}

	const FVector Origin = Params.Origin;

	UE_LOG(LogAshMassSoldier, Log,
		TEXT("AAshMassSoldierSpawner '%s': created %d Mass soldier entities (team %d, squad %d). Spawner now owns %d entities."),
		*GetName(), NumCreated, static_cast<int32>(TeamId), SquadId, SpawnedEntities.Num());

	// Register this spawner's squad with the hierarchical AI (Phase 11). The commander
	// reads squad state to assign objectives; the movement processor steers members toward
	// the assigned objective. Seed the squad's centre of mass with the spawner location so
	// the commander has a meaningful position to plan from before the first aggregation pass.
	if (UAshSquadSubsystem* SquadSubsystem = World->GetSubsystem<UAshSquadSubsystem>())
	{
		SquadSubsystem->RegisterSquad(SquadId, TeamId);
		SquadSubsystem->UpdateSquadAggregate(SquadId, Origin, NumCreated);

		if (UAshCommanderSubsystem* Commander = World->GetSubsystem<UAshCommanderSubsystem>())
		{
			Commander->ReevaluateOrders();
		}
	}

	if (bDrawDebug)
	{
		DrawDebugSoldiers();
	}

	return NumCreated;
}

void AAshMassSoldierSpawner::DespawnSoldiers()
{
	if (SpawnedEntities.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr)
	{
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
		for (const FMassEntityHandle& Entity : SpawnedEntities)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				EntityManager.DestroyEntity(Entity);
			}
		}
	}

	SpawnedEntities.Reset();
}

void AAshMassSoldierSpawner::DrawDebugSoldiers() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	const FColor TeamColor = (TeamId == EAshTeamId::Enemy) ? FColor::Red : FColor::Cyan;

	// Static draw with a long lifetime: Phase 9 soldiers don't move (no processors yet),
	// so a one-shot draw is enough to confirm hundreds of entities exist on screen.
	for (const FMassEntityHandle& Entity : SpawnedEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		const FVector Position = EntityManager.GetFragmentDataChecked<FAshMovementFragment>(Entity).Position;
		DrawDebugPoint(World, Position + FVector(0.f, 0.f, 50.f), 12.f, TeamColor, false, 60.f);
	}
#endif // ENABLE_DRAW_DEBUG
}
