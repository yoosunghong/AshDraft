// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Mass/ExternalSubsystemTraits.h" // TMassFragmentTraits (trivially-copyable opt-out)
#include "Teams/AshTeamTypes.h"
#include "AshSoldierFragments.generated.h"

class AActor;
class UAshSoldierVisualConfig;
class UAshSoldierBehaviorConfig;

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

	/**
	 * Mass-authoritative body yaw (degrees). The movement processor interpolates this toward a
	 * target-aware desired heading each frame (face the combat target while engaging/attacking,
	 * else face travel); the representation proxy reads it to orient the mesh. Decoupling facing
	 * from raw velocity is what lets a stopped, attacking soldier still face its enemy (Phase 20).
	 */
	UPROPERTY()
	float FacingYaw = 0.f;
};

/**
 * Player-displacement guard. BasePosition follows the soldier during normal movement, freezes while
 * the player is physically pushing it, and becomes the forced-return target if the shove exceeds the
 * allowed displacement.
 */
USTRUCT()
struct FAshPlayerDisplacementFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Last non-pushed position; the soldier is forced back here after an excessive player shove. */
	UPROPERTY()
	FVector BasePosition = FVector::ZeroVector;

	/** True while normal steering is overridden to return the soldier to BasePosition. */
	UPROPERTY()
	bool bReturningToBase = false;

	/** World time of the most recent player push. Used to decide when BasePosition may follow again. */
	UPROPERTY()
	float LastPushedTime = -1.e30f;
};

UENUM()
enum class EAshFireteamRole : uint8
{
	Leader,
	Member,
};

UENUM()
enum class EAshFireteamState : uint8
{
	Standby,
	Moving,
	Engaged,
	Reforming,
};

/** Persistent 5-soldier fireteam identity below the general-owned squad. */
USTRUCT()
struct FAshFireteamFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 FireteamId = INDEX_NONE;

	UPROPERTY()
	int32 FireteamIndexInSquad = 0;

	UPROPERTY()
	int32 SlotIndex = 0;

	UPROPERTY()
	int32 FireteamSize = 5;

	UPROPERTY()
	EAshFireteamRole Role = EAshFireteamRole::Member;
};

/** Fireteam-level formation destination. Movement consumes this before the raw squad objective. */
USTRUCT()
struct FAshFormationFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FVector LocalOffset = FVector::ZeroVector;

	UPROPERTY()
	FVector DesiredWorldPosition = FVector::ZeroVector;

	UPROPERTY()
	bool bHasDesiredPosition = false;

	UPROPERTY()
	EAshFireteamState FireteamState = EAshFireteamState::Standby;
};

UENUM()
enum class EAshCombatTargetType : uint8
{
	None,
	MassEntity,
	Actor,
};

/** Higher-level target assignment: supports both Mass soldiers and Actor targets such as generals. */
USTRUCT()
struct FAshCombatTargetFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	EAshCombatTargetType TargetType = EAshCombatTargetType::None;

	UPROPERTY()
	FMassEntityHandle MassTarget;

	UPROPERTY()
	TObjectPtr<AActor> ActorTarget = nullptr;
};

template<>
struct TMassFragmentTraits<FAshCombatTargetFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Local behavior state for a single soldier (Phase 20). This is the soldier's "state tree": a
 * compact, data-oriented FSM evaluated by UAshMassSoldierBehaviorProcessor over packed fragments
 * (the AAA-scale alternative to a per-entity Unreal StateTree, which stays reserved for the few
 * complex General/Squad agents — PROPOSAL.md 15.2/15.3/15.5). Soldiers only act locally: they
 * follow their squad/general objective and engage enemies sensed within a small radius.
 */
UENUM()
enum class EAshSoldierState : uint8
{
	/** No local enemy: advance on the squad's shared objective (the general's command). */
	FollowSquad,

	/** A local enemy is sensed but out of reach: close the distance, facing it. */
	Engage,

	/** A local enemy is within attack range: hold position (anchored) and strike on cooldown. */
	Attack,
};

/** Current FSM state, written by the behavior processor and read by movement/combat (Phase 20). */
USTRUCT()
struct FAshSoldierStateFragment : public FMassFragment
{
	GENERATED_BODY()

	/** The soldier's current local behavior state. */
	UPROPERTY()
	EAshSoldierState State = EAshSoldierState::FollowSquad;

	/**
	 * World position where the soldier began its current engagement. The chase leash
	 * (UAshSoldierBehaviorConfig::MaxLeashFromObjective) is measured from here, so a soldier engages
	 * any enemy that crosses its path during the march yet never wanders far chasing one — it returns
	 * to the flow field once it strays past the leash or the enemy dies (Phase 20.1).
	 */
	UPROPERTY()
	FVector EngageAnchor = FVector::ZeroVector;

	/** True once EngageAnchor is set for the current engagement; cleared on returning to FollowSquad. */
	UPROPERTY()
	bool bEngaged = false;
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

/**
 * One-shot combat animation events surfaced from the combat layer to the representation
 * layer (Phase 15). The combat processor sets bAttackedThisTick on an attacker when its
 * strike lands and bWasHitThisTick on the victim; the representation processor consumes
 * them to play the proxy's attack / hit-react montages, then clears both the same frame so
 * each event animates exactly once. Kept separate from FAshCombatFragment so the combat
 * processor can flag a *target* entity's event via the entity manager without taking write
 * access to the whole combat fragment.
 */
USTRUCT()
struct FAshCombatEventFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Set the tick this soldier landed an attack; drives the proxy attack montage. */
	UPROPERTY()
	bool bAttackedThisTick = false;

	/** Set the tick this soldier took damage; drives the proxy hit-react montage. */
	UPROPERTY()
	bool bWasHitThisTick = false;
};

/**
 * Per-entity link to its unit type's visual set (Phase 15). Seeded by the spawner from
 * UAshMassSoldierConfig::Visual; read by the representation processor to dress the proxy
 * (skeletal mesh, AnimBP, montages) at runtime. The pointer is identical across every entity
 * of one unit type — a Mass *shared* fragment would dedupe it; a plain ObjectPtr fragment is
 * used here for symmetry with the other FAsh* fragments and because the per-entity cost (one
 * pointer) is negligible at PoC scale.
 */
USTRUCT()
struct FAshVisualFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Visual/animation set for this entity's unit type (null = render nothing / Blueprint default). */
	UPROPERTY()
	TObjectPtr<UAshSoldierVisualConfig> Visual = nullptr;
};

/**
 * Mass forbids non-trivially-copyable fragments because it relocates fragment memory with raw
 * memcpy/memmove. In editor builds TObjectPtr carries access-tracking machinery that makes it
 * non-trivially-copyable, even though it is pointer-sized and bit-relocatable (and its referenced
 * UObject stays GC-reachable via the UPROPERTY). We therefore opt out explicitly, mirroring the
 * engine's own TObjectPtr-bearing fragments (see MassEngineMeshFragments.h). The pointer is identical
 * across every entity of a unit type, so a shared fragment would be the dedup-optimal choice; that is
 * deferred per the design note on FAshVisualFragment.
 */
template<>
struct TMassFragmentTraits<FAshVisualFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Per-entity link to its unit type's behavior set (Phase 20). Seeded by the spawner from
 * UAshMassSoldierConfig::Behavior; read by the behavior, movement and ground processors for their
 * data-driven tunables (sense radius, leash, facing turn rate, separation, ground conform). Like
 * FAshVisualFragment, the pointer is identical across every entity of one unit type; a shared
 * fragment would dedupe it, but a plain ObjectPtr fragment keeps symmetry with the other FAsh*
 * fragments at negligible per-entity cost. Null is tolerated: processors fall back to defaults.
 */
USTRUCT()
struct FAshBehaviorFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Behavior/tuning set for this entity's unit type (null = processor fallbacks). */
	UPROPERTY()
	TObjectPtr<const UAshSoldierBehaviorConfig> Behavior = nullptr;
};

/** TObjectPtr opt-out, identical rationale to FAshVisualFragment's (Mass memcpy-relocates fragments). */
template<>
struct TMassFragmentTraits<FAshBehaviorFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
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
