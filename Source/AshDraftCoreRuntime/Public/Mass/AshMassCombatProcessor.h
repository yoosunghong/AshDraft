// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassCombatProcessor.generated.h"

/**
 * UAshMassCombatProcessor
 *
 * Phase 10 data-oriented combat (ARCHITECTURE.md 7.3 / 8.3). Each update it:
 *   1. Builds a coarse uniform spatial grid over all soldier positions (so target
 *      acquisition is roughly O(n) instead of O(n^2)).
 *   2. For each soldier, acquires the nearest *hostile* living soldier within
 *      AcquisitionRadius and records it in FAshCombatFragment::Target.
 *   3. If that target is within AttackRange and the attack cooldown has elapsed, applies
 *      AttackPower damage to the target's FAshHealthFragment (the UAshMassDeathProcessor
 *      removes entities whose health reaches zero).
 *
 * This is the "detect nearby enemy + apply simple damage" soldier rule the architecture
 * asks for — simple local behavior, no per-soldier brain (ARCHITECTURE.md 8.3 / 7.4).
 * The work is throttled per entity by the AI LOD interval (FAshLODFragment), so far-away
 * soldiers fight less frequently (Phase 12). Runs on the game thread; the naive single-
 * threaded grid is a Phase 18 optimization target.
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

	/** Radius (cm) within which a soldier looks for an enemy to engage. Also the grid cell size. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|MassCombat", meta = (ClampMin = "1.0"))
	float AcquisitionRadius = 1500.f;

private:
	FMassEntityQuery EntityQuery;
};
