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

namespace
{
	/**
	 * The 5-soldier V (arrowhead): leader at the tip, two flankers a rank back on each side, two more a
	 * deeper, wider rank behind. Returned in formation-local space (+X = forward); the caller rotates it
	 * to the unit's facing. Kept here so the spawn formation matches the fireteam processor's slots.
	 */
	FVector FireteamSlotOffset(int32 SlotIndex)
	{
		switch (SlotIndex)
		{
		case 0:  return FVector(0.f, 0.f, 0.f);        // tip / leader
		case 1:  return FVector(-150.f, -110.f, 0.f);  // near-left
		case 2:  return FVector(-150.f, 110.f, 0.f);   // near-right
		case 3:  return FVector(-300.f, -220.f, 0.f);  // far-left
		default: return FVector(-300.f, 220.f, 0.f);   // far-right
		}
	}

	/**
	 * Rotates a planar formation-local offset (+X forward, +Y right) onto a world facing direction.
	 * Done by hand (no FRotationMatrix) to stay dependency-free in this translation unit; the convention
	 * matches FRotationMatrix(Yaw).TransformVector so the spawn V lines up with the fireteam re-form V:
	 * the X axis maps to Forward and the Y axis maps to (-Forward.Y, Forward.X).
	 */
	// Distinct name from the fireteam processor's helper: this module uses a unity build, so two
	// anonymous-namespace functions with the same signature would collide in the merged translation unit.
	FVector RotateSpawnOffsetToFacing(const FVector& LocalOffset, const FVector& Forward)
	{
		FVector F = Forward;
		F.Z = 0.f;
		if (!F.Normalize())
		{
			F = FVector::ForwardVector;
		}
		const FVector Right(-F.Y, F.X, 0.f);
		return F * LocalOffset.X + Right * LocalOffset.Y + FVector(0.f, 0.f, LocalOffset.Z);
	}
}

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
		FAshDeathFragment::StaticStruct(),
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
	const float AttackCooldownVariance = Config ? Config->AttackCooldownVariance : Params.FallbackAttackCooldownVariance;

	UAshSoldierVisualConfig* VisualConfig = Config ? Config->Visual : nullptr;
	UAshSoldierBehaviorConfig* BehaviorConfig = Config ? Config->Behavior : nullptr;

	// Data-driven fireteam formation (Phase 26): use the unit type's authored slot offsets if present,
	// else the built-in 5-soldier V. The spawn shape and the fireteam re-form read the same source.
	const auto ResolveSlotOffset = [BehaviorConfig](int32 Slot) -> FVector
	{
		if (BehaviorConfig && BehaviorConfig->FireteamSlotOffsets.IsValidIndex(Slot))
		{
			return BehaviorConfig->FireteamSlotOffsets[Slot];
		}
		return FireteamSlotOffset(Slot);
	};

	const FVector Origin = Params.Origin;
	const FVector Forward = Params.Forward.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Params.Forward.GetSafeNormal2D();
	const int32 FireteamSize = FMath::Max(1, Params.FireteamSize);
	const float SpawnYaw = Forward.Rotation().Yaw;
	// Total fireteams to lay out; used to distribute each one's cluster centre across the deploy area.
	const int32 FireteamCount = FMath::Max(1, FMath::DivideAndRoundUp(Params.Count, FireteamSize));
	// Golden-angle (sunflower) distribution gives an even, non-overlapping spread of cluster centres.
	constexpr float GoldenAngle = 2.39996323f; // radians
	int32 NumCreated = 0;

	OutEntities.Reserve(OutEntities.Num() + Params.Count);
	for (int32 Index = 0; Index < Params.Count; ++Index)
	{
		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);
		OutEntities.Add(Entity);

		const int32 FireteamIndex = Index / FireteamSize;
		const int32 SlotIndex = Index % FireteamSize;

		// Each fireteam gets its own cluster centre spread across the deploy disc (squads start
		// separated, not intermixed). Members sit at their V slot, rotated to the spawn facing.
		const float ClusterRadius = (FireteamCount > 1)
			? Params.SpawnRadius * FMath::Sqrt((FireteamIndex + 0.5f) / FireteamCount)
			: 0.f;
		const float ClusterAngle = FireteamIndex * GoldenAngle;
		const FVector ClusterCentre = Origin + FVector(FMath::Cos(ClusterAngle) * ClusterRadius, FMath::Sin(ClusterAngle) * ClusterRadius, 0.f);
		const FVector Position = ClusterCentre + RotateSpawnOffsetToFacing(ResolveSlotOffset(SlotIndex), Forward);

		EntityManager.GetFragmentDataChecked<FAshTeamFragment>(Entity).TeamId = Params.TeamId;

		FAshHealthFragment& Health = EntityManager.GetFragmentDataChecked<FAshHealthFragment>(Entity);
		Health.MaxHealth = MaxHealth;
		Health.CurrentHealth = MaxHealth;

		FAshMovementFragment& Movement = EntityManager.GetFragmentDataChecked<FAshMovementFragment>(Entity);
		Movement.Position = Position;
		Movement.Velocity = FVector::ZeroVector;
		Movement.MoveSpeed = MoveSpeed;
		Movement.FacingYaw = SpawnYaw; // face the deployment direction from the start

		FAshPlayerDisplacementFragment& Displacement = EntityManager.GetFragmentDataChecked<FAshPlayerDisplacementFragment>(Entity);
		Displacement.BasePosition = Position;

		FAshFireteamFragment& Fireteam = EntityManager.GetFragmentDataChecked<FAshFireteamFragment>(Entity);
		Fireteam.FireteamId = Params.SquadId * 1000 + FireteamIndex;
		Fireteam.FireteamIndexInSquad = FireteamIndex;
		Fireteam.SlotIndex = SlotIndex;
		Fireteam.FireteamSize = FireteamSize;
		Fireteam.Role = (SlotIndex == 0) ? EAshFireteamRole::Leader : EAshFireteamRole::Member;

		// Formation slot matches the spawn V so the fireteam processor re-forms the same shape.
		FAshFormationFragment& Formation = EntityManager.GetFragmentDataChecked<FAshFormationFragment>(Entity);
		Formation.LocalOffset = ResolveSlotOffset(SlotIndex);

		FAshCombatFragment& Combat = EntityManager.GetFragmentDataChecked<FAshCombatFragment>(Entity);
		Combat.AttackRange = AttackRange;
		Combat.AttackPower = AttackPower;
		Combat.AttackCooldown = AttackCooldown;
		Combat.AttackCooldownVariance = FMath::Clamp(AttackCooldownVariance, 0.f, 1.f);
		Combat.RolledAttackInterval = AttackCooldown;
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
