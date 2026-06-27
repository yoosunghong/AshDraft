// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassCombatProcessor.generated.h"

/**
 * UAshMassCombatProcessor
 *
 * Data-oriented combat *damage application* (ARCHITECTURE.md 7.3 / 8.3). As of Phase 20 it no
 * longer searches for targets — the soldier state machine (UAshMassSoldierBehaviorProcessor) owns
 * local target acquisition and the AI time-slice, writing the chosen enemy into
 * FAshCombatFragment::Target. This processor simply, every frame:
 *   1. advances each soldier's attack cooldown timer,
 *   2. resolves its behavior-selected target,
 *   3. if that target is alive, within AttackRange and the cooldown has elapsed, applies AttackPower
 *      to the target's FAshHealthFragment (the UAshMassDeathProcessor removes the dead), and
 *   4. raises one-shot attack/hit events (FAshCombatEventFragment) for the representation layer.
 *
 * This keeps the "simple local damage" rule cheap and decoupled from the (throttled) decision pass,
 * and removes the old map-wide acquisition radius entirely (soldiers act locally — ARCHITECTURE.md
 * 8.3). Runs on the game thread because it writes other entities' fragments via the entity manager.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshMassCombatProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassCombatProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
