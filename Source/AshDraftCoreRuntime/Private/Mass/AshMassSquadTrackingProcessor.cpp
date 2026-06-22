// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassSquadTrackingProcessor.h"

#include "AI/AshSquadSubsystem.h"
#include "Engine/World.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"

UAshMassSquadTrackingProcessor::UAshMassSquadTrackingProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Writes into the squad subsystem (a UObject) on the game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassSquadTrackingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshSquadFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
}

void UAshMassSquadTrackingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = Context.GetWorld();
	UAshSquadSubsystem* SquadSubsystem = World ? World->GetSubsystem<UAshSquadSubsystem>() : nullptr;
	if (!SquadSubsystem)
	{
		return;
	}

	const float WorldTime = World->GetTimeSeconds();
	if (LastAggregateTime >= 0.f && (WorldTime - LastAggregateTime) < AggregateInterval)
	{
		return;
	}
	LastAggregateTime = WorldTime;

	// Accumulate living members per squad.
	struct FAccum { FVector PositionSum = FVector::ZeroVector; int32 Count = 0; };
	TMap<int32, FAccum> Accumulators;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshSquadFragment> SquadList = ChunkContext.GetFragmentView<FAshSquadFragment>();
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			if (HealthList[It].CurrentHealth <= 0.f || SquadList[It].SquadId == INDEX_NONE)
			{
				continue;
			}
			FAccum& Accum = Accumulators.FindOrAdd(SquadList[It].SquadId);
			Accum.PositionSum += MovementList[It].Position;
			++Accum.Count;
		}
	});

	for (const TPair<int32, FAccum>& Pair : Accumulators)
	{
		const FAccum& Accum = Pair.Value;
		const FVector Average = (Accum.Count > 0) ? (Accum.PositionSum / Accum.Count) : FVector::ZeroVector;
		SquadSubsystem->UpdateSquadAggregate(Pair.Key, Average, Accum.Count);
	}
}
