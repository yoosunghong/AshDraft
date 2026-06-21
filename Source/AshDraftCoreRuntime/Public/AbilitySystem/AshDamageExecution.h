// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameplayEffectExecutionCalculation.h"
#include "AshDamageExecution.generated.h"

/**
 * UAshDamageExecution
 *
 * GameplayEffectExecutionCalculation for the basic damage effect
 * (ARCHITECTURE.md 5.3). Captures the source's AttackPower and the target's
 * Defense, computes mitigated damage, and writes it into the target's transient
 * Damage meta attribute (UAshAttributeSet::Damage), which then reduces Health.
 *
 *   Damage = max( SourceAttackPower * Scale - TargetDefense, MinDamage )
 *
 * Keeping the formula in an execution (rather than a flat modifier) lets the
 * damage Gameplay Effect stay a pure data asset while the math is data-tunable
 * and authoritative.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UAshDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
