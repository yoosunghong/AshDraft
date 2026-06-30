// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassCombatProcessor.h"

#include "Character/AshGeneralCharacter.h"
#include "Character/AshHeroCharacter.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"
#include "Templates/Function.h" // TFunctionRef for the per-hit damage callback

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

	// After an attack *cycle* (a 1-3 hit combo) ends, draw the next cooldown from
	// AttackCooldown * [1 - variance, 1 + variance] so soldiers trade blows on staggered timing rather
	// than the whole line swinging in unison. Also clears the combo so the soldier is "idle on cooldown"
	// (Phase 29): the behavior processor then yields its inner attack slot for the duration.
	const auto EndCycle = [](FAshCombatFragment& C)
	{
		const float V = FMath::Clamp(C.AttackCooldownVariance, 0.f, 1.f);
		C.RolledAttackInterval = C.AttackCooldown * FMath::FRandRange(1.f - V, 1.f + V);
		C.TimeSinceLastAttack = 0.f;
		C.ComboLength = 0;
		C.ComboHitsLanded = 0;
		C.TimeSinceComboHit = 0.f;
	};

	// Morale-driven combo length for a fresh cycle (Phase 29): roll the full 3-hit chance first, then the
	// 2-hit chance, else a single hit. Chances are seeded from the owning general's morale; soldiers with
	// no general have 0/0 and always do a single hit.
	const auto RollComboLength = [](const FAshCombatFragment& C) -> int32
	{
		const float P3 = FMath::Clamp(C.ThreeHitChance, 0.f, 1.f);
		const float P2 = FMath::Clamp(C.TwoHitChance, 0.f, 1.f);
		const float R = FMath::FRand();
		if (R < P3)      { return 3; }
		if (R < P3 + P2) { return 2; }
		return 1;
	};

	// Advances one soldier's attack cycle. LandHit(Index) applies this hit's damage (the caller knows
	// whether the target is a Mass entity or an Actor); this owns the timing, combo length and the
	// attacker's one-shot animation event. bInRange gates striking; losing range mid-combo ends the cycle.
	const auto AdvanceCombo = [&](FAshCombatFragment& C, FAshCombatEventFragment& Ev, bool bInRange, const TFunctionRef<void(int32)>& LandHit)
	{
		if (C.ComboLength > 0)
		{
			// Mid-combo: pace the follow-up hits by ComboHitInterval. A target slipping out of range
			// aborts the combo into the cooldown rather than letting the soldier hang mid-swing.
			if (!bInRange)
			{
				EndCycle(C);
				return;
			}
			C.TimeSinceComboHit += DeltaTime;
			if (C.TimeSinceComboHit >= C.ComboHitInterval)
			{
				LandHit(C.ComboHitsLanded);
				Ev.bAttackedThisTick = true;
				Ev.AttackComboIndex = C.ComboHitsLanded;
				++C.ComboHitsLanded;
				C.TimeSinceComboHit = 0.f;
				if (C.ComboHitsLanded >= C.ComboLength)
				{
					EndCycle(C); // whole combo landed -> 3 s cycle cooldown, slot released
				}
			}
			return;
		}

		// Not mid-combo: start a new cycle when in range and the cycle cooldown has elapsed.
		if (bInRange && C.TimeSinceLastAttack >= C.RolledAttackInterval)
		{
			const int32 ComboLen = RollComboLength(C);
			LandHit(0);
			Ev.bAttackedThisTick = true;
			Ev.AttackComboIndex = 0;
			C.ComboHitsLanded = 1;
			C.TimeSinceComboHit = 0.f;
			if (ComboLen <= 1)
			{
				EndCycle(C); // single hit this cycle
			}
			else
			{
				C.ComboLength = ComboLen; // continue with follow-up hits
			}
		}
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
			FAshCombatEventFragment& Event = EventList[It];
			const FAshCombatTargetFragment& CombatTarget = CombatTargetList[It];
			// The cycle cooldown advances every frame; the combo helper gates the next cycle on it.
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
					// Not a damageable actor (or already dead): abort any in-flight combo into cooldown.
					if (Combat.ComboLength > 0) { EndCycle(Combat); }
					continue;
				}

				const float DistSq = FVector::DistSquared2D(MovementList[It].Position, TargetActor->GetActorLocation());
				const bool bInRange = DistSq <= FMath::Square(Combat.AttackRange);
				AdvanceCombo(Combat, Event, bInRange, [&](int32 /*HitIndex*/)
				{
					if (TargetGeneral) { TargetGeneral->ReceiveSoldierDamage(Combat.AttackPower, nullptr); }
					else               { TargetHero->ReceiveSoldierDamage(Combat.AttackPower, nullptr); }
				});
				continue;
			}

			// The behavior processor selected this target (local, leashed). Drop it if it has died or
			// been destroyed since.
			if (!Combat.Target.IsSet() || !EntityManager.IsEntityValid(Combat.Target))
			{
				if (Combat.ComboLength > 0) { EndCycle(Combat); }
				continue;
			}

			const FAshMovementFragment* TargetMovement = EntityManager.GetFragmentDataPtr<FAshMovementFragment>(Combat.Target);
			FAshHealthFragment* TargetHealth = EntityManager.GetFragmentDataPtr<FAshHealthFragment>(Combat.Target);
			if (!TargetMovement || !TargetHealth || TargetHealth->CurrentHealth <= 0.f)
			{
				if (Combat.ComboLength > 0) { EndCycle(Combat); }
				continue;
			}

			// Run the combo cycle: each landed hit deals AttackPower and raises the attacker's attack
			// event (with the combo index for the montage) + the victim's hit-react event (Phase 15/29).
			const float DistSq = FVector::DistSquared2D(MovementList[It].Position, TargetMovement->Position);
			const bool bInRange = DistSq <= FMath::Square(Combat.AttackRange);
			const FMassEntityHandle VictimHandle = Combat.Target;
			AdvanceCombo(Combat, Event, bInRange, [&](int32 /*HitIndex*/)
			{
				TargetHealth->CurrentHealth = FMath::Max(0.f, TargetHealth->CurrentHealth - Combat.AttackPower);
				if (FAshCombatEventFragment* TargetEvent = EntityManager.GetFragmentDataPtr<FAshCombatEventFragment>(VictimHandle))
				{
					TargetEvent->bWasHitThisTick = true;
				}
			});
		}
	});
}
