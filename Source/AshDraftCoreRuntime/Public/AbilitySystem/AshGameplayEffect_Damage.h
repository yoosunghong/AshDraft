// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"
#include "AshGameplayEffect_Damage.generated.h"

/**
 * UAshGameplayEffect_Damage
 *
 * The basic physical damage Gameplay Effect (ARCHITECTURE.md 5.3,
 * GE_Damage_Physical). An instant effect that runs UAshDamageExecution to
 * compute AttackPower-vs-Defense damage into the target's Damage meta attribute.
 *
 * Configured entirely in C++ so the Phase 4 foundation is verifiable without an
 * editor-authored asset; a designer-facing Blueprint child can still be made
 * later if per-attack variants are needed.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshGameplayEffect_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAshGameplayEffect_Damage();
};
