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

	/** True while the soldier is moving above the proxy's facing threshold; gates Idle<->Move. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Anim")
	bool bIsMoving = false;
};
