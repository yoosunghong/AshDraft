// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassSoldierSpawner.h"

#include "AI/AshCommanderSubsystem.h"
#include "AI/AshSquadSubsystem.h"
#include "Engine/World.h"
#include "Mass/AshMassSoldierConfig.h"
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
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogAshMassSoldier, Error,
			TEXT("AAshMassSoldierSpawner '%s': UMassEntitySubsystem unavailable; is the MassEntity plugin enabled?"),
			*GetName());
		return 0;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// 1. Build the soldier archetype from the six data fragments (ARCHITECTURE.md 7.2).
	const TArray<const UScriptStruct*> FragmentTypes =
	{
		FAshTeamFragment::StaticStruct(),
		FAshHealthFragment::StaticStruct(),
		FAshMovementFragment::StaticStruct(),
		FAshCombatFragment::StaticStruct(),
		FAshCombatEventFragment::StaticStruct(),
		FAshSquadFragment::StaticStruct(),
		FAshLODFragment::StaticStruct(),
	};
	const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentTypes);

	// Resolve tunables once (config if assigned, else inline fallbacks).
	const float MaxHealth     = Config ? Config->MaxHealth     : FallbackMaxHealth;
	const float MoveSpeed     = Config ? Config->MoveSpeed     : FallbackMoveSpeed;
	const float AttackRange   = Config ? Config->AttackRange   : FallbackAttackRange;
	const float AttackPower   = Config ? Config->AttackPower   : FallbackAttackPower;
	const float AttackCooldown= Config ? Config->AttackCooldown: FallbackAttackCooldown;

	const FVector Origin = GetActorLocation();
	int32 NumCreated = 0;

	// 2. Create entities and seed their fragments. A simple per-entity create loop is
	// plenty for the foundation's scale; batch APIs are a Phase 10/18 optimization.
	SpawnedEntities.Reserve(SpawnedEntities.Num() + SpawnCount);
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);
		SpawnedEntities.Add(Entity);

		// Scatter across a disc on the spawner's plane.
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = SpawnRadius * FMath::Sqrt(FMath::FRand());
		const FVector Position = Origin + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);

		EntityManager.GetFragmentDataChecked<FAshTeamFragment>(Entity).TeamId = TeamId;

		FAshHealthFragment& Health = EntityManager.GetFragmentDataChecked<FAshHealthFragment>(Entity);
		Health.MaxHealth = MaxHealth;
		Health.CurrentHealth = MaxHealth;

		FAshMovementFragment& Movement = EntityManager.GetFragmentDataChecked<FAshMovementFragment>(Entity);
		Movement.Position = Position;
		Movement.Velocity = FVector::ZeroVector;
		Movement.MoveSpeed = MoveSpeed;

		FAshCombatFragment& Combat = EntityManager.GetFragmentDataChecked<FAshCombatFragment>(Entity);
		Combat.AttackRange = AttackRange;
		Combat.AttackPower = AttackPower;
		Combat.AttackCooldown = AttackCooldown;
		Combat.TimeSinceLastAttack = AttackCooldown; // ready to attack immediately

		FAshSquadFragment& Squad = EntityManager.GetFragmentDataChecked<FAshSquadFragment>(Entity);
		Squad.SquadId = SquadId;
		Squad.OrderId = 0;

		// LOD fragment keeps its struct defaults (LOD 0); the LOD processor (Phase 12) owns it.

		++NumCreated;
	}

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
