// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassCombatProcessor.h"

#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"

UAshMassCombatProcessor::UAshMassCombatProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Targets are chosen by the behavior processor first; damage is applied after.
	ExecutionOrder.ExecuteAfter.Add(TEXT("AshMassSoldierBehaviorProcessor"));
	// Writes other entities' health/event fragments via the entity manager: game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassCombatProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatEventFragment>(EMassFragmentAccess::ReadWrite);
}

void UAshMassCombatProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TArrayView<FAshCombatFragment> CombatList = ChunkContext.GetMutableFragmentView<FAshCombatFragment>();
		const TArrayView<FAshCombatEventFragment> EventList = ChunkContext.GetMutableFragmentView<FAshCombatEventFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			// Dead soldiers do nothing; the death processor will clean them up.
			if (HealthList[It].CurrentHealth <= 0.f)
			{
				continue;
			}

			FAshCombatFragment& Combat = CombatList[It];
			// Cooldown advances every frame; the strike below is naturally rate-limited by it.
			Combat.TimeSinceLastAttack += DeltaTime;

			// The behavior processor selected this target (local, leashed). Drop it if it has died or
			// been destroyed since.
			if (!Combat.Target.IsSet() || !EntityManager.IsEntityValid(Combat.Target))
			{
				continue;
			}

			const FAshMovementFragment* TargetMovement = EntityManager.GetFragmentDataPtr<FAshMovementFragment>(Combat.Target);
			FAshHealthFragment* TargetHealth = EntityManager.GetFragmentDataPtr<FAshHealthFragment>(Combat.Target);
			if (!TargetMovement || !TargetHealth || TargetHealth->CurrentHealth <= 0.f)
			{
				continue;
			}

			// Strike when in range and off cooldown.
			const float DistSq = FVector::DistSquared2D(MovementList[It].Position, TargetMovement->Position);
			if (DistSq <= FMath::Square(Combat.AttackRange) && Combat.TimeSinceLastAttack >= Combat.AttackCooldown)
			{
				TargetHealth->CurrentHealth = FMath::Max(0.f, TargetHealth->CurrentHealth - Combat.AttackPower);
				Combat.TimeSinceLastAttack = 0.f;

				// Surface one-shot animation events for the representation layer (Phase 15):
				// the attacker plays its attack montage, the victim its hit-react montage.
				EventList[It].bAttackedThisTick = true;
				if (FAshCombatEventFragment* TargetEvent = EntityManager.GetFragmentDataPtr<FAshCombatEventFragment>(Combat.Target))
				{
					TargetEvent->bWasHitThisTick = true;
				}
			}
		}
	});
}
