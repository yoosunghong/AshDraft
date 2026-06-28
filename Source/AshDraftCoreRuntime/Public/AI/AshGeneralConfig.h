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

	/** How many soldiers this general commands (spawned on BeginPlay). Data-driven army size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Troops", meta = (ClampMin = "0"))
	int32 TroopCount = 60;

	/** Radius (cm) of the disc the troops are scattered across when first spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General|Troops", meta = (ClampMin = "0.0"))
	float TroopSpawnRadius = 800.f;

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
