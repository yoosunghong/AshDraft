// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Mass/EntityHandle.h"
#include "Teams/AshTeamTypes.h"
#include "AshSoldierProxyActor.generated.h"

class UStaticMeshComponent;

/**
 * AAshSoldierProxyActor
 *
 * Phase 13 hybrid representation (ARCHITECTURE.md 13 / 6.3): a lightweight Actor that gives
 * a *nearby* Mass soldier higher visual fidelity than a debug point. Most soldiers stay as
 * pure Mass entities; only LOD-0 soldiers near the player are "promoted" to one of these
 * proxies, which are pooled and recycled by UAshSoldierProxyPool / the representation
 * processor so the active Actor count stays bounded.
 *
 * The proxy is a dumb view: it has no AI, no Tick, and no combat. It mirrors the entity's
 * position (and team colour / health) each frame from Mass, and can report its transform
 * back when demoted (ARCHITECTURE.md 13.3 state transfer). Mass remains the authority.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshSoldierProxyActor : public AActor
{
	GENERATED_BODY()

public:
	AAshSoldierProxyActor();

	/** Begins representing Entity; makes the proxy visible and records the handle. */
	void AssignToEntity(FMassEntityHandle Entity, EAshTeamId Team);

	/** Stops representing any entity; hides + parks the proxy for reuse by the pool. */
	void ClearAssignment();

	/** The entity this proxy currently represents (unset when pooled/idle). */
	FMassEntityHandle GetRepresentedEntity() const { return RepresentedEntity; }

	/** True while assigned to a live entity. */
	bool IsAssigned() const { return RepresentedEntity.IsSet(); }

	/** Pushes Mass state onto the proxy (ARCHITECTURE.md 13.3: Position, Health). */
	void SyncFromEntity(const FVector& Position, float HealthFraction);

	/** Reads the proxy's current world location back out (for demotion transfer). */
	FVector GetSyncedLocation() const;

protected:
	/** Visible body of the proxied soldier. Assign a mesh in a Blueprint subclass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Proxy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
	/** Handle of the represented entity; unset means this proxy is idle in the pool. */
	FMassEntityHandle RepresentedEntity;

	/** Team currently represented (for colour/state). */
	EAshTeamId RepresentedTeam = EAshTeamId::Neutral;
};
