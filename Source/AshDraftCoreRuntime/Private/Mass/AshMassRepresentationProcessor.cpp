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
	// ...and after the death processor so a soldier that died this frame is already flagged dying and we
	// keep its proxy to play the death montage instead of demoting it (Phase 27).
	ExecutionOrder.ExecuteAfter.Add(TEXT("AshMassDeathProcessor"));
	// Spawns / moves Actors and reads the proxy pool subsystem: game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassRepresentationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshCombatEventFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshDeathFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshVisualFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshLODFragment>(EMassFragmentAccess::ReadOnly);
	// Read-only: surface the soldier's combat state to the proxy so the AnimBP can show a combat stance
	// while engaging / striking / surrounding (Phase 28).
	EntityQuery.AddRequirement<FAshSoldierStateFragment>(EMassFragmentAccess::ReadOnly);
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
		const TArrayView<FAshDeathFragment> DeathList = ChunkContext.GetMutableFragmentView<FAshDeathFragment>();
		const TConstArrayView<FAshVisualFragment> VisualList = ChunkContext.GetFragmentView<FAshVisualFragment>();
		const TConstArrayView<FAshLODFragment> LODList = ChunkContext.GetFragmentView<FAshLODFragment>();
		const TConstArrayView<FAshSoldierStateFragment> StateList = ChunkContext.GetFragmentView<FAshSoldierStateFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(It);
			const bool bAlive = HealthList[It].CurrentHealth > 0.f;
			FAshDeathFragment& Death = DeathList[It];
			// A dying soldier (zero health, still inside its corpse window) keeps the proxy it already had
			// so the death montage can play out before the death processor reaps the entity (Phase 27).
			const bool bDying = !bAlive && Death.bIsDying;
			const bool bNearLOD = LODList[It].LODLevel <= PromoteAtOrBelowLOD;

			if (bAlive && bNearLOD)
			{
				// Promotion: a live, near-player soldier gets a pooled proxy (subject to the cap).
				if (AAshSoldierProxyActor* Proxy = Pool->AcquireProxy(Entity, TeamList[It].TeamId))
				{
					// Dress the (generic, pooled) proxy for this entity's unit type, then mirror state.
					Proxy->ConfigureVisual(VisualList[It].Visual);

					const float MaxHealth = FMath::Max(1.f, HealthList[It].MaxHealth);
					const EAshSoldierState St = StateList[It].State;
					const bool bInCombatStance = (St == EAshSoldierState::Engage
						|| St == EAshSoldierState::Attack || St == EAshSoldierState::Surround);
					Proxy->SyncFromEntity(MovementList[It].Position, MovementList[It].FacingYaw,
						MovementList[It].Velocity, HealthList[It].CurrentHealth / MaxHealth, bInCombatStance);

					// Play this frame's combat events on the visible body (Phase 15).
					if (EventList[It].bAttackedThisTick)
					{
						// Play the montage for this combo hit (Phase 29); the combat processor stamps which hit landed.
						Proxy->PlayAttackMontage(EventList[It].AttackComboIndex);
					}
					if (EventList[It].bWasHitThisTick)
					{
						Proxy->PlayHitReactMontage();
					}
				}
			}
			else if (AAshSoldierProxyActor* Proxy = Pool->FindProxy(Entity))
			{
				if (bDying && bNearLOD)
				{
					// Corpse: keep the proxy this soldier ALREADY held and play the death montage once, then
					// freeze (zero velocity -> idle locomotion params). We never acquire a NEW proxy just for a
					// corpse, so living soldiers keep priority on the bounded pool (Phase 27).
					Proxy->ConfigureVisual(VisualList[It].Visual);
					Proxy->SyncFromEntity(MovementList[It].Position, MovementList[It].FacingYaw,
						FVector::ZeroVector, 0.f);
					if (!Death.bDeathAnimStarted)
					{
						Proxy->PlayDeath();
						Death.bDeathAnimStarted = true;
					}
				}
				else
				{
					// Demotion (live soldier left LOD 0, or a corpse went off-screen / its window ended):
					// transfer the proxy's transform back into Mass, then recycle it.
					MovementList[It].Position = Proxy->GetSyncedLocation();
					Pool->ReleaseProxy(Entity);
				}
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
