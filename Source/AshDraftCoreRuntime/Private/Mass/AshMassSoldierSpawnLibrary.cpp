// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassSoldierSpawnLibrary.h"

#include "Engine/World.h"
#include "Mass/AshMassSoldierConfig.h"
#include "Mass/AshSoldierBehaviorConfig.h"
#include "Mass/AshSoldierFragments.h"
#include "Mass/AshSoldierVisualConfig.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshMassSoldierSpawn, Log, All);

int32 AshMassSoldierSpawn::SpawnSoldiers(
	UWorld* World,
	const FAshSoldierSpawnParams& Params,
	TArray<FMassEntityHandle>& OutEntities)
{
	if (Params.Count <= 0)
	{
		return 0;
	}

	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogAshMassSoldierSpawn, Error,
			TEXT("AshMassSoldierSpawn::SpawnSoldiers: UMassEntitySubsystem unavailable; is the MassEntity plugin enabled?"));
		return 0;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// 1. Build the soldier archetype from the data fragments (ARCHITECTURE.md 7.2). Must match the
	// fragment set the processors query against.
	const TArray<const UScriptStruct*> FragmentTypes =
	{
		FAshTeamFragment::StaticStruct(),
		FAshHealthFragment::StaticStruct(),
		FAshMovementFragment::StaticStruct(),
		FAshPlayerDisplacementFragment::StaticStruct(),
		FAshFireteamFragment::StaticStruct(),
		FAshFormationFragment::StaticStruct(),
		FAshCombatTargetFragment::StaticStruct(),
		FAshCombatFragment::StaticStruct(),
		FAshCombatEventFragment::StaticStruct(),
		FAshVisualFragment::StaticStruct(),
		FAshBehaviorFragment::StaticStruct(),
		FAshSoldierStateFragment::StaticStruct(),
		FAshSquadFragment::StaticStruct(),
		FAshLODFragment::StaticStruct(),
	};
	const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentTypes);

	// Resolve tunables once (config if assigned, else inline fallbacks).
	const UAshMassSoldierConfig* Config = Params.Config;
	const float MaxHealth      = Config ? Config->MaxHealth      : Params.FallbackMaxHealth;
	const float MoveSpeed      = Config ? Config->MoveSpeed      : Params.FallbackMoveSpeed;
	const float AttackRange    = Config ? Config->AttackRange    : Params.FallbackAttackRange;
	const float AttackPower    = Config ? Config->AttackPower    : Params.FallbackAttackPower;
	const float AttackCooldown = Config ? Config->AttackCooldown : Params.FallbackAttackCooldown;

	UAshSoldierVisualConfig* VisualConfig = Config ? Config->Visual : nullptr;
	UAshSoldierBehaviorConfig* BehaviorConfig = Config ? Config->Behavior : nullptr;

	const FVector Origin = Params.Origin;
	int32 NumCreated = 0;

	OutEntities.Reserve(OutEntities.Num() + Params.Count);
	for (int32 Index = 0; Index < Params.Count; ++Index)
	{
		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);
		OutEntities.Add(Entity);

		// Scatter across a disc on the origin's plane.
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = Params.SpawnRadius * FMath::Sqrt(FMath::FRand());
		const FVector Position = Origin + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		constexpr int32 FireteamSize = 5;
		const int32 FireteamIndex = Index / FireteamSize;
		const int32 SlotIndex = Index % FireteamSize;

		EntityManager.GetFragmentDataChecked<FAshTeamFragment>(Entity).TeamId = Params.TeamId;

		FAshHealthFragment& Health = EntityManager.GetFragmentDataChecked<FAshHealthFragment>(Entity);
		Health.MaxHealth = MaxHealth;
		Health.CurrentHealth = MaxHealth;

		FAshMovementFragment& Movement = EntityManager.GetFragmentDataChecked<FAshMovementFragment>(Entity);
		Movement.Position = Position;
		Movement.Velocity = FVector::ZeroVector;
		Movement.MoveSpeed = MoveSpeed;

		FAshPlayerDisplacementFragment& Displacement = EntityManager.GetFragmentDataChecked<FAshPlayerDisplacementFragment>(Entity);
		Displacement.BasePosition = Position;

		FAshFireteamFragment& Fireteam = EntityManager.GetFragmentDataChecked<FAshFireteamFragment>(Entity);
		Fireteam.FireteamId = Params.SquadId * 1000 + FireteamIndex;
		Fireteam.FireteamIndexInSquad = FireteamIndex;
		Fireteam.SlotIndex = SlotIndex;
		Fireteam.FireteamSize = FireteamSize;
		Fireteam.Role = (SlotIndex == 0) ? EAshFireteamRole::Leader : EAshFireteamRole::Member;

		FAshFormationFragment& Formation = EntityManager.GetFragmentDataChecked<FAshFormationFragment>(Entity);
		switch (SlotIndex)
		{
		case 0: Formation.LocalOffset = FVector::ZeroVector; break;
		case 1: Formation.LocalOffset = FVector(-140.f, -90.f, 0.f); break;
		case 2: Formation.LocalOffset = FVector(-140.f, 90.f, 0.f); break;
		case 3: Formation.LocalOffset = FVector(-280.f, -180.f, 0.f); break;
		default: Formation.LocalOffset = FVector(-280.f, 180.f, 0.f); break;
		}

		FAshCombatFragment& Combat = EntityManager.GetFragmentDataChecked<FAshCombatFragment>(Entity);
		Combat.AttackRange = AttackRange;
		Combat.AttackPower = AttackPower;
		Combat.AttackCooldown = AttackCooldown;
		Combat.TimeSinceLastAttack = AttackCooldown; // ready to attack immediately

		EntityManager.GetFragmentDataChecked<FAshVisualFragment>(Entity).Visual = VisualConfig;
		EntityManager.GetFragmentDataChecked<FAshBehaviorFragment>(Entity).Behavior = BehaviorConfig;

		// FAshSoldierStateFragment keeps its default (FollowSquad); the behavior processor owns it.

		FAshSquadFragment& Squad = EntityManager.GetFragmentDataChecked<FAshSquadFragment>(Entity);
		Squad.SquadId = Params.SquadId;
		Squad.OrderId = 0;

		// LOD fragment keeps its struct defaults (LOD 0); the LOD processor owns it.

		++NumCreated;
	}

	return NumCreated;
}
