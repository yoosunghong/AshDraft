// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Mass/EntityHandle.h"
#include "Teams/AshTeamTypes.h"
#include "AshSoldierProxyActor.generated.h"

class USkeletalMeshComponent;
class UAnimMontage;

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
 * position/heading (and team colour / health) each frame from Mass, and can report its
 * transform back when demoted (ARCHITECTURE.md 13.3 state transfer). Mass remains the
 * authority.
 *
 * Phase 15 upgrades the body from a debug/static mesh to a SkeletalMesh so soldiers animate:
 * a Blueprint subclass assigns the skeletal mesh, AnimBP, and the attack / hit-react montages.
 * The representation processor plays those montages in response to the one-shot combat events
 * (FAshCombatEventFragment) the Mass combat processor raises — the proxy itself stays brainless.
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

	/** Pushes Mass state onto the proxy (ARCHITECTURE.md 13.3: Position, heading, Health). */
	void SyncFromEntity(const FVector& Position, const FVector& Velocity, float HealthFraction);

	/** Reads the proxy's current world location back out (for demotion transfer). */
	FVector GetSyncedLocation() const;

	/** Plays the attack montage (no-op if unassigned); driven by a Mass attack event. */
	void PlayAttackMontage();

	/** Plays the hit-react montage (no-op if unassigned); driven by a Mass hit event. */
	void PlayHitReactMontage();

protected:
	/** Skeletal body of the proxied soldier. Assign a mesh + AnimBP in a Blueprint subclass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Proxy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	/** Montage played when this soldier lands an attack. Assign in a Blueprint subclass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Proxy|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Montage played when this soldier is hit. Assign in a Blueprint subclass. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Proxy|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** Min planar speed (cm/s) before the proxy reorients to face its movement direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Proxy", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FacingSpeedThreshold = 10.f;

private:
	/** Handle of the represented entity; unset means this proxy is idle in the pool. */
	FMassEntityHandle RepresentedEntity;

	/** Team currently represented (for colour/state). */
	EAshTeamId RepresentedTeam = EAshTeamId::Neutral;
};
