// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassDeathProcessor.h"

#include "Mass/AshSoldierFragments.h"
#include "MassCommandBuffer.h"
#include "MassExecutionContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshMassDeath, Log, All);

UAshMassDeathProcessor::UAshMassDeathProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Run after combat so this-frame kills are reaped this frame.
	ExecutionOrder.ExecuteAfter.Add(TEXT("AshMassCombatProcessor"));
	// Queues structural changes via the command buffer: game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassDeathProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
}

void UAshMassDeathProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	int32 NumKilled = 0;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			if (HealthList[It].CurrentHealth <= 0.f)
			{
				ChunkContext.Defer().DestroyEntity(ChunkContext.GetEntity(It));
				++NumKilled;
			}
		}
	});

	if (NumKilled > 0)
	{
		UE_LOG(LogAshMassDeath, Verbose, TEXT("UAshMassDeathProcessor: reaped %d dead soldier entities."), NumKilled);
	}
}
