// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassGroundProcessor.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Mass/AshSoldierBehaviorConfig.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"

UAshMassGroundProcessor::UAshMassGroundProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Clamp height after movement integrates XY, but before the proxy reads the position.
	ExecutionOrder.ExecuteAfter.Add(TEXT("AshMassMovementProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("AshMassRepresentationProcessor"));
	// World line traces run on the game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassGroundProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshBehaviorFragment>(EMassFragmentAccess::ReadOnly);
}

void UAshMassGroundProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}

	const FCollisionQueryParams TraceParams(FName(TEXT("AshSoldierGroundConform")), /*bTraceComplex=*/false);

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FAshMovementFragment> MovementList = ChunkContext.GetMutableFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshLODFragment> LODList = ChunkContext.GetFragmentView<FAshLODFragment>();
		const TConstArrayView<FAshBehaviorFragment> BehaviorList = ChunkContext.GetFragmentView<FAshBehaviorFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			if (HealthList[It].CurrentHealth <= 0.f)
			{
				continue;
			}

			const UAshSoldierBehaviorConfig* Cfg = BehaviorList[It].Behavior;
			if (!Cfg || !Cfg->bConformToGround)
			{
				continue; // No conform requested (or no config): leave Z untouched.
			}

			// Only conform visible / near soldiers; distant abstract ones don't need an exact height.
			if (LODList[It].LODLevel > Cfg->MaxConformLOD)
			{
				continue;
			}

			FAshMovementFragment& Movement = MovementList[It];
			const FVector Start = Movement.Position + FVector(0.f, 0.f, Cfg->GroundTraceUp);
			const FVector End = Movement.Position - FVector(0.f, 0.f, Cfg->GroundTraceDown);

			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Start, End, Cfg->GroundTraceChannel.GetValue(), TraceParams))
			{
				Movement.Position.Z = Hit.ImpactPoint.Z;
			}
		}
	});
}
