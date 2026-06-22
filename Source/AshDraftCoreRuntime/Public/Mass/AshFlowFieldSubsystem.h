// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "AshFlowFieldSubsystem.generated.h"

class UAshFlowFieldConfig;

/**
 * UAshFlowFieldSubsystem
 *
 * Phase 14 Flow Field navigation PoC (ARCHITECTURE.md 12.1). It owns a single grid over
 * the battlefield and, on demand, bakes a flow field toward a goal location (a target
 * base): a Dijkstra integration field (cost-to-goal per cell) is solved once, then each
 * cell's best direction is the gradient toward its lowest-cost neighbour. Soldiers then
 * read one shared direction map instead of each running its own pathfind
 * (ARCHITECTURE.md 7.4 / 12: "prefer shared movement fields").
 *
 * Fields are cached per goal *cell*, so the whole army attacking one base shares a single
 * solve and switching target bases (commander re-order, base flip) is just a different
 * cache key — bases are static, so a cached field never goes stale. The Mass movement
 * processor (Phase 10) is the only caller: it asks for a direction in its group-navigation
 * step. No Actor, no Tick — the subsystem is passive and solves lazily (ARCHITECTURE.md 18.3).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshFlowFieldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Optional data-driven tunables; inline defaults are used until one is assigned. */
	void SetConfig(const UAshFlowFieldConfig* InConfig) { Config = InConfig; }

	/**
	 * Returns the shared flow direction (normalised, XY) that steers a soldier at FromLocation
	 * toward a flow field baked for GoalLocation. Lazily builds + caches the field for that
	 * goal cell. Falls back to the straight-line direction when either point is off-grid or the
	 * cell has no downhill neighbour. Returns false only if no usable direction exists.
	 */
	bool GetFlowDirection(const FVector& GoalLocation, const FVector& FromLocation, FVector& OutDirection);

	/** Drops every cached field (e.g. if the grid must be rebuilt). Bases are static, so rarely needed. */
	void InvalidateAll();

	/** Draws the cached flow field(s) as per-cell arrows for one frame. Called by the movement processor. */
	void DrawDebug(const UWorld* World) const;

	/** True once the grid bounds have been resolved from the world's bases. */
	bool IsGridReady() const { return bGridInitialized; }

private:
	/** Lazily resolves grid origin / cell counts / static cost field from the world's bases. */
	void EnsureGridInitialized();

	/** Builds (and caches) the integration + flow field targeting GoalCellIndex. */
	const TArray<FVector2D>& EnsureFieldForGoal(int32 GoalCellIndex);

	/** Solves the Dijkstra integration field (cost-to-goal per cell) from GoalCellIndex. */
	void BuildIntegrationField(int32 GoalCellIndex, TArray<float>& OutIntegration) const;

	/** Bakes per-cell best directions from a solved integration field. */
	void BuildFlowFromIntegration(const TArray<float>& Integration, TArray<FVector2D>& OutFlow) const;

	/** World XY -> cell coords. Returns false (and clamps) when outside the grid. */
	bool WorldToCell(const FVector& World, int32& OutX, int32& OutY) const;

	/** Cell coords -> grid-array index. */
	FORCEINLINE int32 CellIndex(int32 X, int32 Y) const { return Y * CellsX + X; }

	/** Center of a cell in world space (Z = grid plane height). */
	FVector CellCenter(int32 X, int32 Y) const;

	/** Resolves effective tunables from Config or inline fallbacks. */
	float GetCellSize() const;
	int32 GetMaxCachedFields() const;

	/** Active config (weak by raw ptr; only read during solves on the game thread). */
	UPROPERTY(Transient)
	TObjectPtr<const UAshFlowFieldConfig> Config = nullptr;

	bool bGridInitialized = false;

	/** Min corner of the grid in world space; Z is the debug-draw plane height. */
	FVector GridOrigin = FVector::ZeroVector;

	float CellSize = 500.f;
	int32 CellsX = 0;
	int32 CellsY = 0;

	/** Per-cell traversal cost (1 = open, very large = blocked). Built once at grid init. */
	TArray<float> CostField;

	/**
	 * Per-cell ground height (world Z), sampled by a downward trace at grid init. The debug
	 * draw places arrows just above this so the field is visible regardless of where the grid
	 * plane (average base height) falls relative to the terrain. Falls back to GridOrigin.Z
	 * for cells whose trace misses.
	 */
	TArray<float> CellGroundZ;

	/** Goal cell index -> baked per-cell flow directions (XY unit vectors; zero = no flow). */
	TMap<int32, TArray<FVector2D>> FieldCache;
};
