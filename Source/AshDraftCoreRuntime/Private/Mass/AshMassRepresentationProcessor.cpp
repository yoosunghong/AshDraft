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
	// ...and react to this frame's combat so attack / hit montages play the same tick (Phase 15).
	ExecutionOrder.ExecuteAfter.Add(TEXT("AshMassCombatProcessor"));
	// Spawns / moves Actors and reads the proxy pool subsystem: game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassRepresentationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshCombatEventFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshVisualFragment>(EMassFragmentAccess::ReadOnly);
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
		const TArrayView<FAshCombatEventFragment> EventList = ChunkContext.GetMutableFragmentView<FAshCombatEventFragment>();
		const TConstArrayView<FAshVisualFragment> VisualList = ChunkContext.GetFragmentView<FAshVisualFragment>();
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
					// Dress the (generic, pooled) proxy for this entity's unit type, then mirror state.
					Proxy->ConfigureVisual(VisualList[It].Visual);

					const float MaxHealth = FMath::Max(1.f, HealthList[It].MaxHealth);
					Proxy->SyncFromEntity(MovementList[It].Position, MovementList[It].FacingYaw,
						MovementList[It].Velocity, HealthList[It].CurrentHealth / MaxHealth);

					// Play this frame's combat events on the visible body (Phase 15).
					if (EventList[It].bAttackedThisTick)
					{
						Proxy->PlayAttackMontage();
					}
					if (EventList[It].bWasHitThisTick)
					{
						Proxy->PlayHitReactMontage();
					}
				}
			}
			else if (AAshSoldierProxyActor* Proxy = Pool->FindProxy(Entity))
			{
				// Demotion: transfer the proxy's transform back into Mass, then recycle it.
				MovementList[It].Position = Proxy->GetSyncedLocation();
				Pool->ReleaseProxy(Entity);
			}

			// One-shot events are consumed every frame regardless of representation state, so
			// a soldier that is off-screen (or capped out of a proxy) never replays a stale event.
			EventList[It].bAttackedThisTick = false;
			EventList[It].bWasHitThisTick = false;
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
