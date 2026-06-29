// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassDeathProcessor.h"

#include "Engine/World.h"
#include "Mass/AshSoldierFragments.h"
#include "MassCommandBuffer.h"
#include "MassExecutionContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshMassDeath, Log, All);

UAshMassDeathProcessor::UAshMassDeathProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Run after combat so this-frame kills are registered this frame.
	ExecutionOrder.ExecuteAfter.Add(TEXT("AshMassCombatProcessor"));
	// Queues structural changes via the command buffer: game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassDeathProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	// Read-write: this processor owns the dying flag + death timestamp (the representation processor
	// only reads them to drive the montage and sets its own bDeathAnimStarted field).
	EntityQuery.AddRequirement<FAshDeathFragment>(EMassFragmentAccess::ReadWrite);
}

void UAshMassDeathProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = Context.GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const float Window = FMath::Max(0.f, DeathDisplayDuration);
	int32 NumReaped = 0;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TArrayView<FAshDeathFragment> DeathList = ChunkContext.GetMutableFragmentView<FAshDeathFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			if (HealthList[It].CurrentHealth > 0.f)
			{
				continue;
			}

			FAshDeathFragment& Death = DeathList[It];
			if (!Death.bIsDying)
			{
				// First frame at zero health: begin the corpse window (the representation proxy plays the
				// death montage). The entity stays alive-but-inert until the window elapses.
				Death.bIsDying = true;
				Death.DeathTime = Now;
			}
			else if (Now - Death.DeathTime >= Window)
			{
				// Corpse window elapsed: reap the entity (deferred so the structural change is safe).
				ChunkContext.Defer().DestroyEntity(ChunkContext.GetEntity(It));
				++NumReaped;
			}
		}
	});

	if (NumReaped > 0)
	{
		UE_LOG(LogAshMassDeath, Verbose, TEXT("UAshMassDeathProcessor: reaped %d soldier corpses."), NumReaped);
	}
}
