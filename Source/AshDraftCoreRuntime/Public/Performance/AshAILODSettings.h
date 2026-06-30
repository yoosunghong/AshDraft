// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "AshAILODSettings.generated.h"

/**
 * Global AI LOD settings for both Mass soldiers and actor-based non-player heroes.
 *
 * Keep these in DefaultGame.ini / Project Settings instead of per-hero data assets so every general,
 * including Blueprint variants such as Messi, obeys the same performance rules.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Ash AI LOD"))
class ASHDRAFTCORERUNTIME_API UAshAILODSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Distance (cm) at/under which AI is LOD 0 (nearest, highest fidelity). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Distance", meta = (ClampMin = "0.0"))
	float LOD0MaxDistance = 2000.f;

	/** Distance (cm) upper bound for LOD 1; beyond it AI is LOD 2 or 3. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Distance", meta = (ClampMin = "0.0"))
	float LOD1MaxDistance = 6000.f;

	/** Distance (cm) upper bound for LOD 2; beyond it AI is LOD 3 (farthest, cheapest). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Distance", meta = (ClampMin = "0.0"))
	float LOD2MaxDistance = 15000.f;

	/** Seconds between expensive AI updates per LOD level (index = LOD level, 0 = near .. 3 = far). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cadence", meta = (ClampMin = "0.0"))
	TArray<float> UpdateIntervals = { 0.05f, 0.15f, 0.5f, 1.0f };

	/** Population is split into this many batches; one batch's LOD is re-evaluated per frame. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Time Slicing", meta = (ClampMin = "1"))
	int32 NumTimeSliceBatches = 4;

	/** Actor generals at or below this LOD render visuals. 0 matches Mass soldiers: only near LOD is visible. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Actor Visuals", meta = (ClampMin = "0", ClampMax = "3"))
	int32 GeneralVisibleMaxLOD = 0;

	/** Also set ActorHiddenInGame for generals when culled, catching child-actor and Blueprint-added visuals. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Actor Visuals")
	bool bHideGeneralActorWhenCulled = true;

	int32 ComputeLODLevel(float Distance) const;
	float GetUpdateInterval(int32 LODLevel) const;
};
