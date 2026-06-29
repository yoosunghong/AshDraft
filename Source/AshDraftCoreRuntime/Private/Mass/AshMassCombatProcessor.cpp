// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassCombatProcessor.h"

#include "Character/AshGeneralCharacter.h"
#include "Character/AshHeroCharacter.h"
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
	EntityQuery.AddRequirement<FAshCombatTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshCombatEventFragment>(EMassFragmentAccess::ReadWrite);
	// Read-only: a Surround (outer-ring, waiting) soldier menaces but never strikes (Phase 28).
	EntityQuery.AddRequirement<FAshSoldierStateFragment>(EMassFragmentAccess::ReadOnly);
}

void UAshMassCombatProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();

	// After a swing lands, draw the next cooldown from AttackCooldown * [1 - variance, 1 + variance] so
	// soldiers trade blows on staggered timing rather than the whole line swinging in unison.
	const auto RollNextInterval = [](FAshCombatFragment& C)
	{
		const float V = FMath::Clamp(C.AttackCooldownVariance, 0.f, 1.f);
		C.RolledAttackInterval = C.AttackCooldown * FMath::FRandRange(1.f - V, 1.f + V);
		C.TimeSinceLastAttack = 0.f;
	};

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TArrayView<FAshCombatFragment> CombatList = ChunkContext.GetMutableFragmentView<FAshCombatFragment>();
		const TConstArrayView<FAshCombatTargetFragment> CombatTargetList = ChunkContext.GetFragmentView<FAshCombatTargetFragment>();
		const TArrayView<FAshCombatEventFragment> EventList = ChunkContext.GetMutableFragmentView<FAshCombatEventFragment>();
		const TConstArrayView<FAshSoldierStateFragment> StateList = ChunkContext.GetFragmentView<FAshSoldierStateFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			// Dead soldiers do nothing; the death processor will clean them up.
			if (HealthList[It].CurrentHealth <= 0.f)
			{
				continue;
			}

			FAshCombatFragment& Combat = CombatList[It];
			const FAshCombatTargetFragment& CombatTarget = CombatTargetList[It];
			// Cooldown advances every frame; the strike below is naturally rate-limited by it.
			Combat.TimeSinceLastAttack += DeltaTime;

			// Surround soldiers are the waiting outer ring: they hold the cooldown ready (so they can strike
			// the instant they're promoted to an inner slot) but never attack while circling (Phase 28). This
			// is the cinematic cap — only the inner ActiveAttackerCount soldiers around a target swing.
			if (StateList[It].State == EAshSoldierState::Surround)
			{
				continue;
			}

			if (CombatTarget.TargetType == EAshCombatTargetType::Actor)
			{
				// Actor targets are the high-fidelity combatants the soldier layer can damage: a rival
				// general or the player hero. Both expose ReceiveSoldierDamage + IsDead, so resolve which
				// one this is, then strike it the same way.
				AActor* TargetActor = CombatTarget.ActorTarget;
				AAshGeneralCharacter* TargetGeneral = Cast<AAshGeneralCharacter>(TargetActor);
				AAshHeroCharacter* TargetHero = TargetGeneral ? nullptr : Cast<AAshHeroCharacter>(TargetActor);

				const bool bTargetDead = TargetGeneral ? TargetGeneral->IsDead()
					: (TargetHero ? TargetHero->IsDead() : true);
				if (bTargetDead)
				{
					continue; // not a damageable actor, or already dead
				}

				const float DistSq = FVector::DistSquared2D(MovementList[It].Position, TargetActor->GetActorLocation());
				if (DistSq <= FMath::Square(Combat.AttackRange) && Combat.TimeSinceLastAttack >= Combat.RolledAttackInterval)
				{
					if (TargetGeneral)
					{
						TargetGeneral->ReceiveSoldierDamage(Combat.AttackPower, nullptr);
					}
					else
					{
						TargetHero->ReceiveSoldierDamage(Combat.AttackPower, nullptr);
					}
					RollNextInterval(Combat);
					EventList[It].bAttackedThisTick = true;
				}
				continue;
			}

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
			if (DistSq <= FMath::Square(Combat.AttackRange) && Combat.TimeSinceLastAttack >= Combat.RolledAttackInterval)
			{
				TargetHealth->CurrentHealth = FMath::Max(0.f, TargetHealth->CurrentHealth - Combat.AttackPower);
				RollNextInterval(Combat);

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
