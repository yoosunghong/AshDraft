// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassSoldierBehaviorProcessor.generated.h"

/**
 * UAshMassSoldierBehaviorProcessor
 *
 * The Mass soldier's local-AI **state machine** (Phase 20, ARCHITECTURE.md 8.3 / 7.4). It is the
 * data-oriented realization of the soldier "state tree": each AI tick it picks a local target and
 * sets the entity's FAshSoldierStateFragment to FollowSquad / Engage / Attack. The movement,
 * ground, combat and representation processors then act on that state.
 *
 * Soldiers act *locally only*. The commander/squad layer already decides where the army goes
 * (UAshCommanderSubsystem -> UAshSquadSubsystem objective), so this processor does NOT run a
 * map-wide target search. It senses hostiles only within LocalSenseRadius (a small, per-unit-type
 * value) and engages one the instant it enters that radius — combat triggers on contact during the
 * march, not only once the soldier nears its objective (Phase 20.1). To stop soldiers wandering off
 * to chase a straggler, the pursuit is leashed to MaxLeashFromObjective measured from the soldier's
 * *engage anchor* (where it first made contact), not from the distant objective; past the leash it
 * drops the chase and the flow field carries it back to the line.
 *
 * Acquisition uses a coarse uniform spatial hash (O(n)) like the combat processor. The expensive
 * decision is throttled per entity by the AI LOD interval (FAshLODFragment) — this processor owns
 * the AI time-slice; the combat processor was reduced to per-frame damage application driven by the
 * target this processor selects. Runs on the game thread (team statics + squad subsystem).
 * No per-soldier Actor / Tick / brain (ARCHITECTURE.md 18.3).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshMassSoldierBehaviorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassSoldierBehaviorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Local sense radius (cm) used when an entity's behavior config is null. Kept small on purpose. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|SoldierBehavior", meta = (ClampMin = "0.0"))
	float DefaultLocalSenseRadius = 600.f;

private:
	FMassEntityQuery EntityQuery;
};
