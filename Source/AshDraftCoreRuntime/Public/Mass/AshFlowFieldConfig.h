// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshFlowFieldConfig.generated.h"

/**
 * UAshFlowFieldConfig
 *
 * Data-driven tunables for the Phase 14 Flow Field navigation PoC (ARCHITECTURE.md 12.1 /
 * 17). A flow field replaces per-soldier pathfinding with one shared direction map: the
 * battlefield is quantised into a grid, a cost-to-goal (integration) field is computed
 * from a destination base, and the best direction per cell is baked once and read by
 * every soldier heading to that goal (ARCHITECTURE.md 7.4: "prefer shared movement fields").
 *
 * UAshFlowFieldSubsystem consumes these values to size its grid and decide whether to
 * sample static obstacles, so the field can be retuned without recompiling. The subsystem
 * falls back to inline defaults when no config is assigned.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshFlowFieldConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Edge length (cm) of one grid cell. Smaller = finer steering but more cells to solve. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|FlowField|Grid", meta = (ClampMin = "50.0"))
	float CellSize = 500.f;

	/** Extra padding (cm) added around the bounding box of all bases when sizing the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|FlowField|Grid", meta = (ClampMin = "0.0"))
	float GridMargin = 3000.f;

	/** Hard cap on cells per axis; CellSize is grown automatically if the map needs more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|FlowField|Grid", meta = (ClampMin = "4", ClampMax = "512"))
	int32 MaxCellsPerAxis = 128;

	/** Half-extent (cm) of the default grid used when no bases exist to bound it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|FlowField|Grid", meta = (ClampMin = "1000.0"))
	float DefaultGridHalfExtent = 15000.f;

	/**
	 * Sample static world geometry at grid build time and mark blocked cells impassable so
	 * the integration field routes around them. Off by default: an overlap per cell is the
	 * grid's only heavy step, and the open PoC map has no obstacles to avoid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|FlowField|Cost")
	bool bSampleObstacles = false;

	/** Fraction of CellSize used as the obstacle-probe radius when bSampleObstacles is on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|FlowField|Cost", meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float ObstacleProbeFraction = 0.35f;

	/** Most goal-specific fields to cache at once before the cache is cleared (one per target base). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|FlowField", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxCachedFields = 4;
};
