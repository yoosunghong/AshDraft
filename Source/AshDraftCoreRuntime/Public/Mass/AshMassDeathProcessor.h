// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassDeathProcessor.generated.h"

/**
 * UAshMassDeathProcessor
 *
 * Phase 10 death handling (ARCHITECTURE.md 8.3 "die when health reaches zero"). It scans
 * soldier entities and queues destruction for any whose FAshHealthFragment has dropped to
 * zero, via the deferred command buffer (Context.Defer().DestroyEntity) so the structural
 * change happens safely after processing. Death is event-driven off the health value, not
 * polled per-Actor (ARCHITECTURE.md 15 / 18.3).
 *
 * Ordered to run after the combat processor so a soldier killed this frame is removed the
 * same frame. Removing the entity also invalidates the handle, so other soldiers' cached
 * FAshCombatFragment::Target naturally drops on their next combat tick.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshMassDeathProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassDeathProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
