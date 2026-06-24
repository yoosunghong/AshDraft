// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshMassLODConfig.generated.h"

/**
 * UAshMassLODConfig
 *
 * Data-driven tunables for the Phase 12 AI LOD + time slicing system (ARCHITECTURE.md 9).
 * Promotes the distance thresholds, per-LOD update cadence, and time-slice batch count off
 * the UAshMassLODProcessor CDO and onto an editable asset, so RL / automated perf runs can
 * sweep LOD aggressiveness (e.g. tighten the rings, change far-soldier update rate) without
 * recompiling or editing the processor defaults.
 *
 * UAshMassLODProcessor resolves this asset once (via a config-persisted soft pointer) and
 * falls back to its inline ctor defaults when no config is assigned, so existing behaviour
 * is unchanged until a config is wired in.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshMassLODConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Distance (cm) at/under which a soldier is LOD 0 (nearest, highest fidelity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassLOD|Distance", meta = (ClampMin = "0.0"))
	float LOD0MaxDistance = 2000.f;

	/** Distance (cm) upper bound for LOD 1; beyond it a soldier is LOD 2 or 3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassLOD|Distance", meta = (ClampMin = "0.0"))
	float LOD1MaxDistance = 6000.f;

	/** Distance (cm) upper bound for LOD 2; beyond it a soldier is LOD 3 (farthest, cheapest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassLOD|Distance", meta = (ClampMin = "0.0"))
	float LOD2MaxDistance = 15000.f;

	/**
	 * Seconds between expensive (combat) updates per LOD level — the combat processor skips a
	 * soldier until WorldTime - LastUpdateTime >= this. Index = LOD level (0 = near .. 3 = far).
	 * Set every entry to 0 to disable throttling (full-rate combat) for the perf baseline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassLOD|Cadence", meta = (ClampMin = "0.0"))
	TArray<float> LODUpdateIntervals = { 0.05f, 0.15f, 0.5f, 1.0f };

	/** Population is split into this many batches; one batch's LOD is re-evaluated per frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassLOD|TimeSlice", meta = (ClampMin = "1"))
	int32 NumTimeSliceBatches = 4;
};
