// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassRepresentationProcessor.h"

#include "Engine/World.h"
#include "Mass/AshSoldierFragments.h"
#include "Mass/AshSoldierProxyActor.h"
#include "Mass/AshSoldierProxyPool.h"
#include "MassExecutionContext.h"

UAshMassRepresentationProcessor::UAshMassRepresentationProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Proxies should follow the position movement produced this frame.
	ExecutionOrder.ExecuteAfter.Add(TEXT("AshMassMovementProcessor"));
	// Spawns / moves Actors and reads the proxy pool subsystem: game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassRepresentationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshLODFragment>(EMassFragmentAccess::ReadOnly);
}

void UAshMassRepresentationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = Context.GetWorld();
	UAshSoldierProxyPool* Pool = World ? World->GetSubsystem<UAshSoldierProxyPool>() : nullptr;
	if (!Pool)
	{
		return;
	}

	// Keep the pool's tuning in sync with this processor's CDO config.
	Pool->ConfigurePool(ProxyClass, MaxActiveProxies);

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FAshMovementFragment> MovementList = ChunkContext.GetMutableFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshLODFragment> LODList = ChunkContext.GetFragmentView<FAshLODFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(It);
			const bool bAlive = HealthList[It].CurrentHealth > 0.f;
			const bool bWantsProxy = bAlive && (LODList[It].LODLevel <= PromoteAtOrBelowLOD);

			if (bWantsProxy)
			{
				// Promotion: ensure a proxy exists (subject to the cap) and mirror Mass onto it.
				if (AAshSoldierProxyActor* Proxy = Pool->AcquireProxy(Entity, TeamList[It].TeamId))
				{
					const float MaxHealth = FMath::Max(1.f, HealthList[It].MaxHealth);
					Proxy->SyncFromEntity(MovementList[It].Position, HealthList[It].CurrentHealth / MaxHealth);
				}
			}
			else if (AAshSoldierProxyActor* Proxy = Pool->FindProxy(Entity))
			{
				// Demotion: transfer the proxy's transform back into Mass, then recycle it.
				MovementList[It].Position = Proxy->GetSyncedLocation();
				Pool->ReleaseProxy(Entity);
			}
		}
	});

	// Sweep proxies whose entity no longer exists (killed this frame / destroyed elsewhere).
	TArray<FMassEntityHandle> ActiveEntities;
	Pool->GetActiveEntities(ActiveEntities);
	for (const FMassEntityHandle& Entity : ActiveEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			Pool->ReleaseProxy(Entity);
		}
	}
}
