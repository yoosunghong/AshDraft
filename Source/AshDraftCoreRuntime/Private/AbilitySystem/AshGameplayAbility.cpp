// Copyright YoosungHong. All Rights Reserved.

#include "AbilitySystem/AshGameplayAbility.h"

UAshGameplayAbility::UAshGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// One instance per actor: melee abilities keep simple per-avatar state and
	// can run ability tasks (montage/timers) without per-activation allocation.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Single-player PoC executes locally but stays authority-friendly
	// (ARCHITECTURE.md 5.5); abilities run on the avatar's authority/prediction.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}
