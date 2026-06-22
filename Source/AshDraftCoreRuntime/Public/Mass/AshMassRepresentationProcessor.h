// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "Templates/SubclassOf.h"
#include "AshMassRepresentationProcessor.generated.h"

class AAshSoldierProxyActor;

/**
 * UAshMassRepresentationProcessor
 *
 * Phase 13 Mass<->Actor hybrid representation (ARCHITECTURE.md 13 / 6.3). Each frame it
 * applies the promotion/demotion rules to every soldier entity:
 *   - Promotion: an alive, LOD-0 (near-player) soldier is given a pooled
 *     AAshSoldierProxyActor and its position is pushed onto that Actor.
 *   - Demotion: a soldier that drops out of LOD 0 has its proxy's transform copied back
 *     into Mass (state transfer, ARCHITECTURE.md 13.3) and the proxy returned to the pool.
 *   - Destroyed entities are swept and their proxies recycled.
 * The pool caps the active proxy count, so only the nearby slice of a large army ever
 * pays Actor cost (ARCHITECTURE.md 13: "important things precise, large-scale things
 * cheap"). Runs after movement so proxies follow the latest positions; no Actor Tick.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshMassRepresentationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassRepresentationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** LOD level (inclusive) at or below which a soldier is promoted to an Actor proxy. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Representation", meta = (ClampMin = "0", ClampMax = "3"))
	int32 PromoteAtOrBelowLOD = 0;

	/** Hard cap on simultaneously-active proxies, forwarded to UAshSoldierProxyPool. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Representation", meta = (ClampMin = "0"))
	int32 MaxActiveProxies = 100;

	/** Proxy actor class to spawn (assign a Blueprint with a mesh for visible soldiers). */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Representation")
	TSubclassOf<AAshSoldierProxyActor> ProxyClass;

private:
	FMassEntityQuery EntityQuery;
};
