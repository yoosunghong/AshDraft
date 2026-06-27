// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h" // ECollisionChannel
#include "AshSoldierBehaviorConfig.generated.h"

/**
 * UAshSoldierBehaviorConfig
 *
 * The data-driven *behavior set* for one Mass soldier unit type (Phase 20, ARCHITECTURE.md 7.4 / 17).
 * Where UAshMassSoldierConfig holds a unit's *stats* and UAshSoldierVisualConfig holds its *look*,
 * this asset holds how it *behaves locally*: how far it senses enemies, how far it will chase from
 * its squad's objective, how fast it turns, how it spaces from squad-mates, and how it sits on the
 * ground. Every tunable that used to be a hardcoded processor constant lives here so designers tune
 * soldiers as data (CLAUDE.md: no hardcoded gameplay values).
 *
 * It is referenced from UAshMassSoldierConfig::Behavior, stamped onto each entity via
 * FAshBehaviorFragment by the spawner, and read by the behavior / movement / ground processors.
 * The processors keep sane fallbacks, so this asset is optional (null -> defaults).
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshSoldierBehaviorConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// --- Local sensing & leash (soldiers act locally, never run a map-wide search) ---

	/**
	 * Radius (cm) within which a soldier notices a hostile and switches from following its squad to
	 * engaging. Deliberately small: target selection is a *local* rule; the squad/general decides
	 * where the army goes (ARCHITECTURE.md 8.3).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Sensing", meta = (ClampMin = "0.0"))
	float LocalSenseRadius = 600.f;

	/**
	 * Max distance (cm) a soldier may stray from where it *began an engagement* while chasing a local
	 * enemy. A marching soldier engages any hostile that enters LocalSenseRadius immediately (combat
	 * triggers on contact, not only near the objective), but it only pursues this far from its engage
	 * anchor before giving up and letting the flow field carry it back — so soldiers fight what crosses
	 * their path yet never peel off to chase a straggler across the map. 0 disables the leash (chase
	 * any sensed enemy indefinitely). (Phase 20.1: measured from the engage anchor, not the objective.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Sensing", meta = (ClampMin = "0.0"))
	float MaxLeashFromObjective = 1200.f;

	// --- Facing (fixes "attacks while facing the wrong way") ---

	/**
	 * How fast (deg/s) the body yaws toward its desired heading. Facing is target-aware (face the
	 * enemy while engaging/attacking, else face travel) and interpolated at this rate so turns read
	 * naturally instead of snapping.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Facing", meta = (ClampMin = "0.0"))
	float TurnRateDegPerSec = 720.f;

	// --- Separation / anti-shaking (fixes jitter + larger army shoving the smaller) ---

	/** Personal-space radius (cm); same-team neighbours closer than this push apart. ~ body diameter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Separation", meta = (ClampMin = "0.0"))
	float SeparationRadius = 90.f;

	/** Push magnitude relative to MoveSpeed; the combined steer+separation velocity is clamped to MoveSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Separation", meta = (ClampMin = "0.0"))
	float SeparationStrength = 0.6f;

	/**
	 * Fraction of the computed push applied per frame (0..1). Values < 1 make a crowd *relax* into a
	 * stable spacing instead of two soldiers shoving each other back and forth — this is the primary
	 * cure for the "5+ soldiers shake" jitter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Separation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeparationRelaxation = 0.5f;

	/**
	 * Caps a single neighbour's push contribution so two soldiers at near-zero distance cannot fling
	 * each other (the (1 - dist/radius) term -> 1 as distance -> 0; this clamps it).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Separation", meta = (ClampMin = "0.0"))
	float MaxPushPerNeighbor = 1.f;

	/**
	 * How strongly an *attacking* soldier resists being pushed (0..1). While in the Attack state the
	 * received separation push is scaled by (1 - this), so front-line soldiers hold their ground.
	 * Together with same-team-only separation this stops a denser formation from bulldozing the
	 * thinner enemy line ("attacks while pushing").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Separation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CombatAnchorResistance = 0.85f;

	/**
	 * Distance (cm) from a *group* objective (squad objective / target base) within which a soldier
	 * eases its speed in proportion to the remaining distance, instead of ramming the goal at full
	 * speed. Because many soldiers share one objective point they cannot all occupy it; without this
	 * ease-in the ones squeezed out re-accelerate back at full speed every frame and the crowd shakes.
	 * Paired with the distributed-deployment stop (a soldier holds once a squad-mate already fills the
	 * space ahead), it lets a cluster settle into a stable, spaced formation. 0 disables the ease-in.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Separation", meta = (ClampMin = "0.0"))
	float ArrivalSlowdownRadius = 250.f;

	// --- Ground conform (fixes "half buried in the ground") ---

	/** When true, sample terrain height under the soldier and clamp its Z so it stands on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Ground")
	bool bConformToGround = true;

	/** Highest (numerically) LOD level still conformed; visible soldiers only by default (LOD 0). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Ground", meta = (ClampMin = "0"))
	int32 MaxConformLOD = 0;

	/** Trace channel used to find the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	/** Start the ground trace this far (cm) above the soldier's current position. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Ground", meta = (ClampMin = "0.0"))
	float GroundTraceUp = 200.f;

	/** Trace this far (cm) downward looking for ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Ground", meta = (ClampMin = "0.0"))
	float GroundTraceDown = 1000.f;
};
