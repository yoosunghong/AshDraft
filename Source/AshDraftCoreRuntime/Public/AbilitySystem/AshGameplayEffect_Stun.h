// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"
#include "AshGameplayEffect_Stun.generated.h"

/**
 * UAshGameplayEffect_Stun
 *
 * The stun state Gameplay Effect (ARCHITECTURE.md 5.3, GE_State_Stunned). A HasDuration effect that grants
 * the Ash.State.Stunned tag for its lifetime, then removes it automatically when it expires — replacing the
 * earlier loose-tag + manual-timer approach so stun is a first-class GAS state. The duration is a
 * SetByCaller magnitude (tag Ash.Data.StunDuration) so the applier supplies it: today from the victim's
 * HitReactStunDuration, later from the attacker's weapon / skill data, with no change here.
 *
 * Abilities that must not run while stunned list Ash.State.Stunned in their ActivationBlockedTags (the
 * basic attack does); movement code reads the tag to block input.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshGameplayEffect_Stun : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAshGameplayEffect_Stun();
};
