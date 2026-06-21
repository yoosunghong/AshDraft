// Copyright YoosungHong. All Rights Reserved.

#include "AbilitySystem/AshDamageExecution.h"

#include "AbilitySystem/AshAttributeSet.h"

namespace AshDamageExecutionConstants
{
	// Minimum damage a connecting hit always deals, so high Defense cannot fully
	// negate an attack. PoC tuning value (ARCHITECTURE.md 17 would move this to a
	// data asset if combat depth grows).
	static constexpr float MinDamage = 1.f;
}

/** Attribute capture definitions for the damage calculation. */
struct FAshDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Defense);

	FAshDamageStatics()
	{
		// AttackPower from the source (attacker); snapshot at GE application.
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAshAttributeSet, AttackPower, Source, true);
		// Defense from the target (victim); read live at execution time.
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAshAttributeSet, Defense, Target, false);
	}
};

static const FAshDamageStatics& DamageStatics()
{
	static FAshDamageStatics Statics;
	return Statics;
}

UAshDamageExecution::UAshDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().DefenseDef);
}

void UAshDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float AttackPower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackPowerDef, EvalParams, AttackPower);

	float Defense = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DefenseDef, EvalParams, Defense);

	const float Damage = FMath::Max(AttackPower - Defense, AshDamageExecutionConstants::MinDamage);

	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(UAshAttributeSet::GetDamageAttribute(), EGameplayModOp::Additive, Damage));
}
