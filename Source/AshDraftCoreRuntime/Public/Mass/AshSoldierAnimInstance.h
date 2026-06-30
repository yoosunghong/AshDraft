// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "AshSoldierAnimInstance.generated.h"

/**
 * UAshSoldierAnimInstance
 *
 * Animation interface for a Mass soldier proxy (Phase 15). The proxy is a passive view moved by
 * AAshSoldierProxyActor::SyncFromEntity with SetActorLocation and has NO movement component, so a
 * blueprint AnimBP cannot read locomotion off GetVelocity() (it is always zero). Instead the proxy
 * pushes the entity's Mass-authoritative ground speed onto these fields each frame, and the AnimBP
 * drives its Idle<->Locomotion state machine / blendspace from them.
 *
 * Reparent your minion AnimBP to this class (Class Settings -> Parent Class) to expose GroundSpeed /
 * bIsMoving in the AnimGraph. The fields are written from C++ (the proxy) so they are read-only to
 * Blueprint; they are plain replicated-free state because the proxy is a cosmetic, locally simulated
 * view (Mass remains the authority).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshSoldierAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Planar ground speed (cm/s) pushed from Mass each frame; drives the locomotion blendspace. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Anim")
	float GroundSpeed = 0.f;

	/**
	 * Signed angle (degrees, -180..180) between the soldier's *travel* direction and the direction it is
	 * *facing*. 0 = moving where it looks (walk forward), ±180 = moving backwards (the Dynasty-Warriors
	 * kiting backpedal — a soldier gives ground while still facing the enemy), ±90 = strafing. Feed this
	 * to the X (Direction) axis of a 2D locomotion blendspace so backpedalling plays a backward-walk clip
	 * instead of a forward one. Same semantics as UKismetAnimationLibrary::CalculateDirection.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Anim")
	float Direction = 0.f;

	/** True while the soldier is moving above the proxy's facing threshold; gates Idle<->Move. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Anim")
	bool bIsMoving = false;

	/**
	 * True while the soldier is engaged in melee — closing on, striking, or surrounding/menacing a target
	 * (Phase 28). Drives an optional combat-stance: a soldier in this state should hold its weapon up and
	 * threaten (the "combat idle" of the back-rank waiting soldiers that sells the crowded Musou battlefield)
	 * rather than play the relaxed out-of-combat idle. Branch your AnimBP's Idle pose on this to get the
	 * guard pose; locomotion (GroundSpeed/Direction) is unaffected, so a circling soldier strafes in stance.
	 * Optional polish — the surround behavior itself works without any AnimBP change.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Anim")
	bool bInCombatStance = false;

	/**
	 * True once this soldier has died (Phase 29). Set by the proxy alongside the death montage; drives an
	 * AnimBP **Dead state** that holds the downed pose for the corpse window (the death montage plays the
	 * motion into it, then blends out — so the static held pose must come from this state, not the montage).
	 * Cleared when the pooled proxy is recycled for a live soldier, so the body animates normally again.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Anim")
	bool bIsDead = false;
};
