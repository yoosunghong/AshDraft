// Copyright YoosungHong. All Rights Reserved.

#include "AbilitySystem/AshGameplayEffect_Damage.h"

#include "AbilitySystem/AshDamageExecution.h"

UAshGameplayEffect_Damage::UAshGameplayEffect_Damage()
{
	// Instant: a one-shot hit, applied authoritatively and immediately consumed
	// by the attribute set's PostGameplayEffectExecute (no duration/period).
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Run the AttackPower-vs-Defense calculation; it writes the Damage meta attribute.
	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UAshDamageExecution::StaticClass();
	Executions.Add(ExecDef);
}
