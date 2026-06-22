// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshSoldierProxyPool.h"

#include "Engine/World.h"
#include "Mass/AshSoldierProxyActor.h"

void UAshSoldierProxyPool::Deinitialize()
{
	for (const TPair<FMassEntityHandle, TObjectPtr<AAshSoldierProxyActor>>& Pair : ActiveProxies)
	{
		if (Pair.Value)
		{
			Pair.Value->Destroy();
		}
	}
	for (AAshSoldierProxyActor* Proxy : IdleProxies)
	{
		if (Proxy)
		{
			Proxy->Destroy();
		}
	}
	ActiveProxies.Reset();
	IdleProxies.Reset();

	Super::Deinitialize();
}

void UAshSoldierProxyPool::ConfigurePool(TSubclassOf<AAshSoldierProxyActor> InProxyClass, int32 InMaxActiveProxies)
{
	ProxyClass = InProxyClass ? InProxyClass : AAshSoldierProxyActor::StaticClass();
	MaxActiveProxies = FMath::Max(0, InMaxActiveProxies);
}

AAshSoldierProxyActor* UAshSoldierProxyPool::SpawnProxy()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const TSubclassOf<AAshSoldierProxyActor> ClassToSpawn = ProxyClass ? ProxyClass : AAshSoldierProxyActor::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAshSoldierProxyActor>(ClassToSpawn, FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

AAshSoldierProxyActor* UAshSoldierProxyPool::AcquireProxy(FMassEntityHandle Entity, EAshTeamId Team)
{
	// Already represented?
	if (const TObjectPtr<AAshSoldierProxyActor>* Existing = ActiveProxies.Find(Entity))
	{
		return *Existing;
	}

	// Respect the active cap (ARCHITECTURE.md 13: bounded Actor count).
	if (ActiveProxies.Num() >= MaxActiveProxies)
	{
		return nullptr;
	}

	// Reuse an idle proxy, else spawn a new one.
	AAshSoldierProxyActor* Proxy = nullptr;
	while (IdleProxies.Num() > 0 && Proxy == nullptr)
	{
		Proxy = IdleProxies.Pop();
	}
	if (!Proxy)
	{
		Proxy = SpawnProxy();
	}
	if (!Proxy)
	{
		return nullptr;
	}

	Proxy->AssignToEntity(Entity, Team);
	ActiveProxies.Add(Entity, Proxy);
	return Proxy;
}

void UAshSoldierProxyPool::ReleaseProxy(FMassEntityHandle Entity)
{
	TObjectPtr<AAshSoldierProxyActor> Proxy;
	if (ActiveProxies.RemoveAndCopyValue(Entity, Proxy) && Proxy)
	{
		Proxy->ClearAssignment();
		IdleProxies.Add(Proxy);
	}
}

AAshSoldierProxyActor* UAshSoldierProxyPool::FindProxy(FMassEntityHandle Entity) const
{
	const TObjectPtr<AAshSoldierProxyActor>* Found = ActiveProxies.Find(Entity);
	return Found ? *Found : nullptr;
}

void UAshSoldierProxyPool::GetActiveEntities(TArray<FMassEntityHandle>& OutEntities) const
{
	OutEntities.Reset(ActiveProxies.Num());
	ActiveProxies.GetKeys(OutEntities);
}
