// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshGeneralConfig.generated.h"

class UAshMassSoldierConfig;
class UStateTree;

/**
 * UAshGeneralConfig
 *
 * The data-driven definition of one General "archetype" (Phase 22, ARCHITECTURE.md 17 / CLAUDE.md:
 * no hardcoded gameplay values). It bundles everything that makes a general behave the way it does:
 * how many troops it commands and of what unit type, how tightly those troops form up, how far it
 * will detour to react to threats in its path, which StateTree drives its decisions, and how its
 * think-rate falls off with distance (actor AI-LOD).
 *
 * AAshGeneralCharacter points at one of these. Adding a general type (heavy infantry commander,
 * cavalry captain, …) is a new asset of this class — no code or Blueprint change.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshGeneralConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// --- Troops the general commands ---

	/** Unit type the general spawns for its squad. Null -> the spawn library's inline fallbacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Troops")
	TObjectPtr<UAshMassSoldierConfig> SoldierConfig;

	/**
	 * Number of 5-soldier squads (fireteams) this general commands. The actual soldier body is
	 * TroopCount * 5 — e.g. TroopCount = 5 spawns five V-shaped squads (25 soldiers). Each squad
	 * deploys as its own cluster within TroopSpawnRadius so the army starts spread out, not piled up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Troops", meta = (ClampMin = "0"))
	int32 TroopCount = 6;

	/** Radius (cm) of the area the squads are distributed across when first spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Troops", meta = (ClampMin = "0.0"))
	float TroopSpawnRadius = 1200.f;

	/**
	 * Radius (cm) the troops form up within around the general (the "certain range around the General"
	 * the soldiers may move in). Published as the squad's FormationRadius each operational update, so
	 * when the general holds, soldiers ring it rather than piling on its exact point; when it moves,
	 * they follow as a body. Soldiers may still leave this range briefly to engage (Phase 20 leash),
	 * then return.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Troops", meta = (ClampMin = "0.0"))
	float FormationRadius = 700.f;

	// --- Operational behavior (StateTree sub-objective triggers) ---

	/** Distance (cm) the general is considered "arrived" at its commander objective. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Behavior", meta = (ClampMin = "0.0"))
	float ObjectiveAcceptanceRadius = 400.f;

	/**
	 * Distance (cm) within which a hostile actor preempts the commander order so the general engages it
	 * (highest-priority sub-objective: "encountering an enemy while moving"). The general resumes its
	 * order once the threat is gone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Behavior", meta = (ClampMin = "0.0"))
	float EnemyEngageRadius = 1500.f;

	/**
	 * Distance (cm) within which a hostile base preempts the commander order so the general assaults it
	 * (mid-priority sub-objective: "an enemy stronghold in the path"). Resolves when the base is no
	 * longer hostile, then the general resumes its order.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Behavior", meta = (ClampMin = "0.0"))
	float StrongholdDetourRadius = 2500.f;

	/** Reach (cm) at which the general triggers its melee attack on a sensed enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Behavior", meta = (ClampMin = "0.0"))
	float AttackRange = 175.f;

	/** Seconds a general may tunnel on the player before it must look for another nearby target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Behavior", meta = (ClampMin = "0.0"))
	float PlayerChaseRetargetSeconds = 5.f;

	/** Radius (cm) used when finding a replacement hostile after an extended player chase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Behavior", meta = (ClampMin = "0.0"))
	float PlayerChaseRetargetRadius = 2200.f;

	/** Formation radius (cm) used while soldiers are being pulled into an active general-vs-general fight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Troops", meta = (ClampMin = "0.0"))
	float CombatFormationRadius = 250.f;

	// --- Global Combat / Engagement Director (Phase 24) ---

	/**
	 * Distance (cm) at which this general's approach to a hostile general triggers a **Battle** (the
	 * global combat state). On encounter, UAshBattleSubsystem assigns every fireteam a designated enemy
	 * squad + a deployment slot on a ring around the battle centre, and the soldiers march straight to
	 * it. Larger = the clash forms from further out. Read as max() across the two clashing generals.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Battle", meta = (ClampMin = "0.0"))
	float BattleEncounterRadius = 3000.f;

	/**
	 * Radius (cm) of the circle the squad duels are spread around. Each duel gets an evenly-spaced
	 * angular slot at this radius from the battle centre, so the melee forms a ring of simultaneous
	 * 1v1/1v2 fights (Dynasty-Warriors style) instead of one long grinding line. Read as max() across
	 * the clashing generals.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Battle", meta = (ClampMin = "0.0"))
	float DuelRingRadius = 1400.f;

	/**
	 * Cap on how many friendly fireteams may pile onto one enemy fireteam when sides are unequal. The
	 * surplus side doubles up onto the nearest under-cap enemy (1v1 -> 1v2), never 1v5 — so duels stay
	 * roughly symmetric. 2 = allow up to a 1v2. Read as max() across the clashing generals.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Battle", meta = (ClampMin = "1"))
	int32 MaxAttackersPerEnemyFireteam = 2;

	/**
	 * How many of the general's *closest* fireteams stay as a personal guard instead of being sent out to
	 * the duel ring (Phase 27). Without this every fireteam rings out at DuelRingRadius and the generals
	 * end up dueling alone in an empty clearing; the guard keeps a knot of soldiers fighting *around* the
	 * general. These fireteams get no ring assignment, so they follow the general's own combat objective
	 * and engage whatever is near it (the enemy's guards / the rival general). 0 = the old behaviour
	 * (everyone rings out). Applied per general (each keeps its own nearest fireteams).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Battle", meta = (ClampMin = "0"))
	int32 GuardFireteamCount = 1;

	/**
	 * Smoothing (0..1) applied to the battle centre between replans. The centre is a low-pass filter of
	 * the participating generals' mean, so the duel ring drifts smoothly instead of snapping each replan
	 * (a snapping centre was part of why committed squads appeared to march back and forth). Lower = more
	 * stable / laggier; 1 = follow the generals instantly. Read as max() across the clashing generals.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Battle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RingRecenterSmoothing = 0.25f;

	/**
	 * Seconds between engagement replans. Fast enough to react to a wiped squad, slow enough that each
	 * fireteam commits to its duel + ring slot between replans. The subsystem adapts its timer to the
	 * max() of this across the clashing generals (a constant fallback applies before any general exists).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Battle", meta = (ClampMin = "0.05"))
	float ReplanPeriod = 0.5f;

	// --- Decision logic ---

	/** StateTree asset that drives the general's operational decisions (assign in the editor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|AI")
	TObjectPtr<UStateTree> StateTree;

	// --- Actor AI-LOD (think-rate throttle by distance to player; ARCHITECTURE.md 9) ---

	/** Distance (cm) upper bounds for LOD 0 / 1 / 2; beyond the last value the general is LOD 3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|LOD", meta = (ClampMin = "0.0"))
	float LOD0MaxDistance = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|LOD", meta = (ClampMin = "0.0"))
	float LOD1MaxDistance = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|LOD", meta = (ClampMin = "0.0"))
	float LOD2MaxDistance = 20000.f;

	/**
	 * Seconds between general "thinks" per LOD level (index = LOD level, 0 = near .. 3 = far). Drives
	 * both the StateTree component tick interval and the cached threat-sensing refresh, so far
	 * generals decide at ~1-2 Hz and near ones at ~30-60 Hz (ARCHITECTURE.md 9.3). 0 = every frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|LOD", meta = (ClampMin = "0.0"))
	TArray<float> ThinkIntervals = { 0.033f, 0.1f, 0.5f, 1.0f };
};
