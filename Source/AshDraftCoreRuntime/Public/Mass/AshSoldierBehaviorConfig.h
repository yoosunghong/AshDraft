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

	// --- Combat spacing (soldiers close to striking range and hold; no backpedal) ---

	/**
	 * Preferred striking distance as a fraction of the unit's AttackRange. < 1 so the soldier closes
	 * just inside attack range to land a hit, then holds there (it never steers backward — same-team
	 * separation seats the spacing). Also floored by MinCombatSpacing so bodies never fully overlap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Combat", meta = (ClampMin = "0.0"))
	float AttackStandoffScale = 0.85f;

	/** Absolute floor (cm) on the striking standoff so opposing soldiers keep their bodies apart in melee. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Combat", meta = (ClampMin = "0.0"))
	float MinCombatSpacing = 110.f;

	// --- Melee dissolve (Phase 26: distinct soldier-vs-soldier pairing, jumbled front, rear engagement) ---

	/**
	 * Max soldiers of one side that may COMMIT to the SAME enemy soldier (its full attack ring capacity:
	 * inner strikers + outer waiters) before the surplus must pick a different (open) enemy. Once a target
	 * is full the overflow is forced toward deeper, unclaimed enemies (organic flanking / rear engagement).
	 *
	 * Interplay with ActiveAttackerCount (Phase 28): the first ActiveAttackerCount committers strike from
	 * the inner ring; committers beyond that (up to this cap) take the outer Surround ring and only menace.
	 * So a small value (e.g. 2) keeps a 30v30 army pairing off 1v1/1v2 (no surround); a larger value (6-8)
	 * lets soldiers form a real surround ring where they outnumber targets (around the hero / a lone
	 * general / the last survivor) — the Musou crowd.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Melee", meta = (ClampMin = "1"))
	int32 MaxAttackersPerEnemySoldier = 4;

	// --- Surround / attack-slot ring (Phase 28: the "few attack, the rest circle and menace" Musou look) ---

	/**
	 * How many soldiers around any ONE target actually strike (the inner ring). The rest of the committers
	 * (up to MaxAttackersPerEnemySoldier) take the outer Surround ring: they hold a wider radius, slowly
	 * orbit, face the target and threaten, but do NOT attack until an inner slot frees up. This is the
	 * cinematic cap from the brief — "only 3 to 5 soldiers actually attack while the rest circle". Clamped
	 * to MaxAttackersPerEnemySoldier. When >= the cap, every committer strikes (the pre-Phase-28 behavior).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Melee", meta = (ClampMin = "1"))
	int32 ActiveAttackerCount = 3;

	/**
	 * Extra radius (cm) the outer Surround ring sits beyond a striker's standoff distance. The outer ring
	 * is always pushed past attack range so waiting soldiers physically can't reach the target — they ring
	 * it. Larger = a looser, more visible surround; smaller = the crowd presses in tighter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Melee", meta = (ClampMin = "0.0"))
	float SurroundRingGap = 150.f;

	/**
	 * How fast (deg/s) a waiting (Surround) soldier orbits its target. Gives the back ranks the restless
	 * circle-around motion that makes the battlefield read as crowded and chaotic instead of statically
	 * queued. 0 = the surround ring holds still (soldiers just wait their turn in place).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Melee", meta = (ClampMin = "0.0"))
	float SurroundOrbitSpeed = 35.f;

	/**
	 * Radius (cm) the soldier-vs-soldier brawl may wander from its fireteam's *contact anchor* (the duel
	 * ring slot) once engaged in a battle. Keeps the jumble a local bubble around each contact point —
	 * soldiers may cross the original line to reach an open enemy, but the squad does not disperse across
	 * the map. Falls back to MaxLeashFromObjective (anchored to the engage point) outside a battle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Melee", meta = (ClampMin = "0.0"))
	float MeleeChaseRadius = 700.f;

	/** Distance (cm) bonus that keeps a soldier on its current melee target — anti-thrash hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Melee", meta = (ClampMin = "0.0"))
	float MeleeTargetStickiness = 150.f;

	/**
	 * Personal-space radius (cm) applied against ENEMY soldiers at close range. 0 (default) reproduces the
	 * old hard standoff (opposing lines never overlap). A small non-zero value lets opposing bodies slip
	 * past each other so the front lines blend/jumble instead of forming a clean wall, while still
	 * preventing perfect overlap. Keep it well under SeparationRadius (anti-stack only, not anti-approach).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Melee", meta = (ClampMin = "0.0"))
	float EnemySeparationRadius = 0.f;

	/** Push magnitude (relative to MoveSpeed) of the enemy anti-stack. Only matters when EnemySeparationRadius > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Melee", meta = (ClampMin = "0.0"))
	float EnemySeparationStrength = 0.35f;

	// --- Fireteam formation (data-driven V; replaces the hardcoded spawn offsets) ---

	/**
	 * Formation-local slot offsets (+X forward, +Y right) for the soldiers of one fireteam — the spawn
	 * shape AND the re-form shape read the same source. Empty = the spawn library's built-in 5-soldier V
	 * arrowhead. Authoring more/larger formations is now pure data (CLAUDE.md: no hardcoded gameplay values).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|Formation")
	TArray<FVector> FireteamSlotOffsets;

	// --- Hit reaction (Phase 32: pushed back + stunned on being hit) ---

	/**
	 * Seconds a soldier is stunned when struck: it can neither move nor attack while the stun is active
	 * (the Mass-soldier equivalent of the Ash.State.Stunned GAS state the hero/general use). A fresh hit
	 * refreshes the window. 0 disables the flinch entirely for this unit type.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|HitReact", meta = (ClampMin = "0.0"))
	float StunDuration = 0.35f;

	/**
	 * Initial knockback speed (cm/s) away from the attacker on being hit — "pushed back ever so slightly".
	 * The shove decays to zero over StunDuration (movement processor), so the total push is small. 0 = no
	 * knockback (stun only).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Behavior|HitReact", meta = (ClampMin = "0.0"))
	float KnockbackSpeed = 250.f;

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
	float SeparationRadius = 120.f;

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
	float CombatAnchorResistance = 0.6f;

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
