// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "Teams/AshTeamTypes.h"
#include "AshMassSoldierSpawner.generated.h"

class UAshMassSoldierConfig;
struct FMassArchetypeHandle;

/**
 * AAshMassSoldierSpawner
 *
 * The Phase 9 Mass soldier "archetype + spawner" (PLAN Phase 9). Placed in a level, on
 * BeginPlay it:
 *   1. Builds a Mass *archetype* from the six FAsh* soldier fragments (the data-oriented
 *      replacement for the Phase 8 ACharacter soldier; ARCHITECTURE.md 7.1-7.2).
 *   2. Batch-creates SpawnCount entities of that archetype directly through the
 *      FMassEntityManager (no per-soldier Actor, no Tick, no Behavior Tree; 7.4 / 18.3).
 *   3. Seeds each entity's fragments from UAshMassSoldierConfig and scatters them across
 *      a disc so the army occupies the battlefield.
 *
 * Phase 9 deliberately adds no processors — it only proves that hundreds/thousands of
 * soldier entities can exist cheaply. Movement, combat, LOD and representation are the
 * Mass Processors added in Phases 10/12/13. The spawner owns its entities and destroys
 * them on EndPlay so PIE restarts cleanly.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshMassSoldierSpawner : public AActor
{
	GENERATED_BODY()

public:
	AAshMassSoldierSpawner();

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	/** Spawns SpawnCount soldier entities now. Returns the number actually created. */
	UFUNCTION(BlueprintCallable, Category = "Ash|MassSoldier")
	int32 SpawnSoldiers();

	/** Destroys every entity this spawner created. */
	UFUNCTION(BlueprintCallable, Category = "Ash|MassSoldier")
	void DespawnSoldiers();

	/** Number of live entities this spawner currently owns. */
	UFUNCTION(BlueprintPure, Category = "Ash|MassSoldier")
	int32 GetSpawnedCount() const { return SpawnedEntities.Num(); }

	/** Desired count the next (re)spawn will create. */
	UFUNCTION(BlueprintPure, Category = "Ash|MassSoldier")
	int32 GetDesiredSpawnCount() const { return SpawnCount; }

	/** Sets how many soldiers the next (re)spawn creates (Phase 18 perf-harness driver). */
	UFUNCTION(BlueprintCallable, Category = "Ash|MassSoldier")
	void SetDesiredSpawnCount(int32 InCount) { SpawnCount = FMath::Max(0, InCount); }

	/** Despawns all current entities then spawns SpawnCount fresh ones. Returns the new count. */
	UFUNCTION(BlueprintCallable, Category = "Ash|MassSoldier")
	int32 Respawn();

protected:
	/** Data-driven tuning seeded into each entity's fragments. Inline defaults used if null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier")
	TObjectPtr<UAshMassSoldierConfig> Config;

	/** Team assigned to every soldier this spawner creates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier")
	EAshTeamId TeamId = EAshTeamId::Ally;

	/** Squad id stamped on every soldier (single squad per spawner for the foundation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier")
	int32 SquadId = 0;

	/** How many soldier entities to create. Push this into the hundreds/thousands to verify scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier", meta = (ClampMin = "0"))
	int32 SpawnCount = 300;

	/** Radius (cm) of the disc around the spawner that soldiers are scattered across. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier", meta = (ClampMin = "0.0"))
	float SpawnRadius = 2000.f;

	/** Create the soldiers automatically on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier")
	bool bSpawnOnBeginPlay = true;

	/** Draw a point per entity in PIE so the army is visible (no processors render them yet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Debug")
	bool bDrawDebug = true;

	/** Inline fallbacks mirroring UAshMassSoldierConfig defaults, used when Config is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Fallback", meta = (ClampMin = "1.0"))
	float FallbackMaxHealth = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Fallback", meta = (ClampMin = "0.0"))
	float FallbackMoveSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Fallback", meta = (ClampMin = "0.0"))
	float FallbackAttackRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Fallback", meta = (ClampMin = "0.0"))
	float FallbackAttackPower = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Fallback", meta = (ClampMin = "0.05"))
	float FallbackAttackCooldown = 1.5f;

private:
	/** Draws one debug point per spawned entity for verification. */
	void DrawDebugSoldiers() const;

	/** Handles of the entities created by this spawner (it owns their lifetime). */
	TArray<FMassEntityHandle> SpawnedEntities;
};
