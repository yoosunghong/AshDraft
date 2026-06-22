// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassSquadTrackingProcessor.generated.h"

/**
 * UAshMassSquadTrackingProcessor
 *
 * Phase 11 squad aggregation (ARCHITECTURE.md 8.2 "track average squad position / alive
 * count"). It is the bridge from the data-oriented layer back up to the strategic layer:
 * each pass it sums living members' positions per SquadId and writes the average position
 * and alive count into UAshSquadSubsystem, where the commander and movement processor read
 * them.
 *
 * Squad-level state changes slowly relative to the frame rate, so the aggregation only
 * runs every AggregateInterval seconds rather than every frame (a coarse, squad-wide form
 * of the time slicing in ARCHITECTURE.md 9.4).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshMassSquadTrackingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassSquadTrackingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Seconds between squad aggregation passes. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Squad", meta = (ClampMin = "0.0"))
	float AggregateInterval = 0.5f;

private:
	FMassEntityQuery EntityQuery;

	/** World time of the last aggregation pass; gates AggregateInterval. */
	float LastAggregateTime = -1.f;
};
