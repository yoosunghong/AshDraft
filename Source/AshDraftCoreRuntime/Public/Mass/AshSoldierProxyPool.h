// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Teams/AshTeamTypes.h"
#include "Templates/SubclassOf.h"
#include "AshSoldierProxyPool.generated.h"

class AAshSoldierProxyActor;

/**
 * UAshSoldierProxyPool
 *
 * Phase 13 proxy lifetime manager (ARCHITECTURE.md 13.1/13.2 promotion & demotion). It
 * owns a bounded pool of AAshSoldierProxyActor instances and hands them out to entities
 * that the representation processor promotes (LOD 0), recycling them when entities are
 * demoted or destroyed. Capping the active count keeps the Actor cost flat no matter how
 * large the Mass army grows — only the nearby slice is ever represented as Actors.
 *
 * The pool is the runtime worker; tuning (proxy class, max active) is supplied by the
 * representation processor via ConfigurePool, so the configuration stays data/CDO-driven
 * without the subsystem needing a per-level asset reference.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshSoldierProxyPool : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~UWorldSubsystem interface
	virtual void Deinitialize() override;
	//~End of UWorldSubsystem interface

	/** Sets the proxy class to spawn and the maximum simultaneously-active proxy count. */
	void ConfigurePool(TSubclassOf<AAshSoldierProxyActor> InProxyClass, int32 InMaxActiveProxies);

	/**
	 * Returns the proxy already assigned to Entity, else activates a pooled/new one (up to
	 * the cap), else nullptr when the active limit is reached.
	 */
	AAshSoldierProxyActor* AcquireProxy(FMassEntityHandle Entity, EAshTeamId Team);

	/** Returns Entity's proxy to the idle pool (no-op if it has none). */
	void ReleaseProxy(FMassEntityHandle Entity);

	/** The proxy currently representing Entity, or nullptr. */
	AAshSoldierProxyActor* FindProxy(FMassEntityHandle Entity) const;

	/** Number of currently-active (assigned) proxies. */
	UFUNCTION(BlueprintPure, Category = "Ash|Proxy")
	int32 GetActiveProxyCount() const { return ActiveProxies.Num(); }

	/** Snapshot of the entities that currently hold a proxy (for the processor's sweep). */
	void GetActiveEntities(TArray<FMassEntityHandle>& OutEntities) const;

private:
	/** Spawns a fresh proxy actor (idle/hidden). Returns nullptr if the world/class is bad. */
	AAshSoldierProxyActor* SpawnProxy();

	/** Entity -> its active proxy. */
	UPROPERTY(Transient)
	TMap<FMassEntityHandle, TObjectPtr<AAshSoldierProxyActor>> ActiveProxies;

	/** Hidden, reusable proxies waiting to be re-assigned. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AAshSoldierProxyActor>> IdleProxies;

	/** Class spawned for new proxies (defaults to AAshSoldierProxyActor). */
	UPROPERTY(Transient)
	TSubclassOf<AAshSoldierProxyActor> ProxyClass;

	/** Hard cap on simultaneously-active proxies (ARCHITECTURE.md 13: bounded Actor cost). */
	int32 MaxActiveProxies = 100;
};
