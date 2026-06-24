// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassLODProcessor.generated.h"

class UAshMassLODConfig;

/**
 * UAshMassLODProcessor
 *
 * Phase 12 AI LOD + time slicing (ARCHITECTURE.md 9). It classifies each soldier into an
 * LOD level (0 = near/high-fidelity ... 3 = far/abstract) from its distance to the player
 * pawn, then writes the matching update interval into FAshLODFragment. The combat
 * processor reads that interval to throttle expensive work, so far soldiers fight less
 * often (ARCHITECTURE.md 9.3).
 *
 * The LOD recompute is itself time-sliced: only 1/NumTimeSliceBatches of the population is
 * re-evaluated each frame (ARCHITECTURE.md 9.4), since a soldier's LOD changes slowly. An
 * optional debug pass draws one LOD-coloured point per soldier every frame — this also
 * doubles as the only on-screen visualisation of Phase 10 movement until the Phase 13
 * representation proxies exist.
 *
 * Camera-visibility and objective-importance LOD factors (ARCHITECTURE.md 9.2) are stubbed
 * for now (distance only); see Known Issues in the DONE doc.
 *
 * Thresholds / intervals / batch count default to the values set in the ctor, but can be
 * overridden by a UAshMassLODConfig data asset (resolved once on first Execute) so RL /
 * automated perf runs can sweep LOD aggressiveness without recompiling. config=Game persists
 * the LODConfig soft pointer to DefaultGame.ini.
 */
// config=Game persists the LODConfig soft pointer below to DefaultGame.ini under
// [/Script/AshDraftCoreRuntime.AshMassLODProcessor]; a non-config native CDO has no on-disk
// persistence, so the asset assignment would reset on every editor launch.
UCLASS(config = Game)
class ASHDRAFTCORERUNTIME_API UAshMassLODProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassLODProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/**
	 * Optional data-driven override for the thresholds / intervals / batch count below. When
	 * assigned, its values are copied into the working fields on the first Execute; otherwise
	 * the inline ctor defaults are used. Lets perf runs retune LOD without recompiling.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassLOD")
	TSoftObjectPtr<UAshMassLODConfig> LODConfig;

	/** Distance (cm) upper bounds for LOD 0/1/2; beyond the last value a soldier is LOD 3. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|MassLOD", meta = (ClampMin = "0.0"))
	float LOD0MaxDistance = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ash|MassLOD", meta = (ClampMin = "0.0"))
	float LOD1MaxDistance = 6000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ash|MassLOD", meta = (ClampMin = "0.0"))
	float LOD2MaxDistance = 15000.f;

	/** Seconds between expensive updates for each LOD level (index = LOD level). Set in ctor. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|MassLOD", meta = (ClampMin = "0.0"))
	float LODUpdateIntervals[4];

	/** Population is split into this many batches; one batch is re-evaluated per frame. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|MassLOD", meta = (ClampMin = "1"))
	int32 NumTimeSliceBatches = 4;

	/** Draw one LOD-coloured point per soldier each frame (also visualises movement). */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|MassLOD|Debug")
	bool bDrawLODDebug = true;

private:
	/** Maps a player-distance to an LOD level using the configured thresholds. */
	int32 ComputeLODLevel(float Distance) const;

	/** Copies LODConfig's values into the working fields once (no-op if unassigned). */
	void ResolveConfig();

	FMassEntityQuery EntityQuery;

	/** Guards ResolveConfig so the asset is loaded/applied a single time. */
	bool bConfigResolved = false;

	/** Advances each Execute; selects which time-slice batch is re-evaluated this frame. */
	int32 FrameCounter = 0;
};
