// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Teams/AshTeamTypes.h"
#include "AshSoldierFragments.generated.h"

/**
 * AshSoldierFragments.h
 *
 * The Phase 9 data-oriented soldier fragments (ARCHITECTURE.md 7.2). Each soldier is a
 * Mass Entity: an ID associated with these compact POD fragments rather than an
 * ACharacter with an independent brain. The layout intentionally mirrors the scalar
 * fields used by the Phase 8 prototype AAshSoldierCharacter / UAshSoldierConfig so the
 * port from Actor soldiers to Mass soldiers is mechanical.
 *
 * These are plain data structs (USTRUCT deriving from FMassFragment) with no behavior
 * and therefore no .cpp; they are grouped in one header because Mass fragments are
 * trivial data definitions, not the substantial classes the one-class-per-file rule
 * (18.2) targets. Behavior lives in Mass Processors (Phase 10), never here.
 */

/** Team identity. Mirrors AAshSoldierCharacter::TeamId; cheap to compare in batches (7.4). */
USTRUCT()
struct FAshTeamFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Battlefield team this soldier belongs to. */
	UPROPERTY()
	EAshTeamId TeamId = EAshTeamId::Neutral;
};

/** Authoritative health pool. Maps to UAshSoldierConfig::MaxHealth. */
USTRUCT()
struct FAshHealthFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Current health; death processing triggers when this reaches zero (Phase 10). */
	UPROPERTY()
	float CurrentHealth = 0.f;

	/** Full / initial health. */
	UPROPERTY()
	float MaxHealth = 0.f;
};

/**
 * Position / velocity / speed for the soldier.
 *
 * Phase 9 stores Position directly (per ARCHITECTURE.md 7.2) so the foundation is
 * self-contained and verifiable without representation plumbing. The Mass movement /
 * representation processors (Phases 10/13) may later migrate this onto the engine's
 * FTransformFragment, but the field set stays the same.
 */
USTRUCT()
struct FAshMovementFragment : public FMassFragment
{
	GENERATED_BODY()

	/** World-space location of the soldier. */
	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	/** Current velocity, written by the movement processor (Phase 10). */
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	/** Max ground speed (cm/s). Maps to UAshSoldierConfig::MoveSpeed. */
	UPROPERTY()
	float MoveSpeed = 0.f;
};

/** Combat state and tunables. Maps to UAshSoldierConfig attack fields. */
USTRUCT()
struct FAshCombatFragment : public FMassFragment
{
	GENERATED_BODY()

	/**
	 * Current engagement target. The conceptual "TargetEntityId" from ARCHITECTURE.md 7.2,
	 * stored as a real Mass handle so the combat processor (Phase 10) can resolve it.
	 * Invalid handle = no target.
	 */
	UPROPERTY()
	FMassEntityHandle Target;

	/** Distance (cm) within which the soldier may strike its target. */
	UPROPERTY()
	float AttackRange = 0.f;

	/** Seconds between attacks. */
	UPROPERTY()
	float AttackCooldown = 0.f;

	/** Damage applied per successful attack. */
	UPROPERTY()
	float AttackPower = 0.f;

	/** Seconds elapsed since the last attack; the combat processor gates against AttackCooldown. */
	UPROPERTY()
	float TimeSinceLastAttack = 0.f;
};

/** Squad membership and current order. Drives hierarchical AI (Phase 11). */
USTRUCT()
struct FAshSquadFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Owning squad id; INDEX_NONE = unassigned. */
	UPROPERTY()
	int32 SquadId = INDEX_NONE;

	/** Current squad order id (e.g. attack/defend), assigned by Squad AI later. */
	UPROPERTY()
	int32 OrderId = 0;
};

/** AI LOD / update-rate bookkeeping. Consumed by the LOD processor (Phase 12). */
USTRUCT()
struct FAshLODFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 0 = near/high-fidelity ... 3 = far/abstract (ARCHITECTURE.md 9.1). */
	UPROPERTY()
	int32 LODLevel = 0;

	/** Desired seconds between updates for this LOD level. */
	UPROPERTY()
	float UpdateInterval = 0.1f;

	/** World time (s) of the last update; used for time slicing (9.4). */
	UPROPERTY()
	float LastUpdateTime = 0.f;
};
