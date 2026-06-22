// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassMovementProcessor.generated.h"

/**
 * UAshMassMovementProcessor
 *
 * Phase 10 data-oriented movement (ARCHITECTURE.md 7.3 / 12). Every frame it steers each
 * soldier entity and integrates its position from velocity. Steering priority:
 *   1. A valid combat target (FAshCombatFragment::Target) -> move toward that entity.
 *   2. The squad's shared objective (UAshSquadSubsystem) -> advance on the objective.
 *   3. Fallback: the nearest non-friendly base (UAshBaseSubsystem) -> so the army has
 *      somewhere to go even before the commander assigns squad orders (Phase 11).
 * When already within attack range of the target the soldier stops so the combat
 * processor can strike (no overlap-piling beyond range; finer slot spacing is the
 * combat-slot system's job, ARCHITECTURE.md 10).
 *
 * Integration runs every frame because it is cheap; the *expensive* decisions
 * (target acquisition) are throttled in the combat processor via AI LOD (Phase 12).
 * No per-soldier Actor, Tick, or pathfinding (ARCHITECTURE.md 7.4 / 18.3).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshMassMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Distance (cm) at which a soldier is considered "arrived" and stops advancing. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|MassMovement", meta = (ClampMin = "0.0"))
	float ArrivalTolerance = 100.f;

private:
	FMassEntityQuery EntityQuery;
};
