// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassDeathProcessor.generated.h"

/**
 * UAshMassDeathProcessor
 *
 * Phase 10 death handling (ARCHITECTURE.md 8.3 "die when health reaches zero"), extended in Phase 27
 * with a corpse window so death animations can play. It scans soldier entities and, the first frame a
 * FAshHealthFragment drops to zero, flips FAshDeathFragment::bIsDying and stamps DeathTime instead of
 * destroying the entity outright. The representation processor then plays the unit's death montage and
 * holds the corpse. Once DeathDisplayDuration seconds have elapsed the entity is reaped via the deferred
 * command buffer (Context.Defer().DestroyEntity) so the structural change happens safely after processing.
 *
 * A dying (zero-health) entity is inert: every gameplay processor already gates on CurrentHealth > 0, so
 * it neither moves, fights, nor is targeted while the corpse lingers — it only animates. Death stays
 * event-driven off the health value, not polled per-Actor (ARCHITECTURE.md 15 / 18.3). Ordered after the
 * combat processor so a kill is registered the same frame; reaping invalidates the handle, so other
 * soldiers' cached FAshCombatFragment::Target naturally drops on their next combat tick.
 */
UCLASS(config = Game)
class ASHDRAFTCORERUNTIME_API UAshMassDeathProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassDeathProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/**
	 * Seconds a dead soldier's body lingers (playing its death montage) before the entity is reaped.
	 * Data-driven (CLAUDE.md: no hardcoded gameplay values); the brief's "removed after 5 seconds".
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Death", meta = (ClampMin = "0.0"))
	float DeathDisplayDuration = 5.f;

private:
	FMassEntityQuery EntityQuery;
};
