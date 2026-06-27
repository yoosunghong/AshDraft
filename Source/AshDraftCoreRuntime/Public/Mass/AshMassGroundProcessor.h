// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassGroundProcessor.generated.h"

/**
 * UAshMassGroundProcessor
 *
 * Conforms a soldier's height to the terrain (Phase 20). Mass soldiers integrate in the XY plane and
 * previously kept whatever Z they spawned at (the spawner's flat height), so on any slope or uneven
 * ground they ended up buried or floating — and the mesh's pelvis->feet art offset alone could not
 * fix it. This processor line-traces down beneath each *visible* soldier (LOD <= the config's
 * MaxConformLOD) and clamps FAshMovementFragment::Position.Z onto the hit, so soldiers stand on the
 * ground. The per-mesh art offset (UAshSoldierVisualConfig::MeshRelativeLocation.Z) still handles
 * local pelvis alignment; this handles world height.
 *
 * Conforming only the visible (proxied) soldiers keeps the trace count bounded (they are capped by
 * the representation pool) — combat uses 2D distance, so far/abstract soldiers do not need an exact
 * Z. Runs after movement (so it clamps the integrated position) and before representation (so the
 * proxy reads the conformed height). Game thread: it performs world line traces.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshMassGroundProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassGroundProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
