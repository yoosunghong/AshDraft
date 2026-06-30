// Copyright YoosungHong. All Rights Reserved.

#include "AbilitySystem/AshGameplayEffect_Stun.h"

#include "AshGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UAshGameplayEffect_Stun::UAshGameplayEffect_Stun()
{
	// Lasts a (caller-supplied) duration, then expires and removes the granted tag on its own — no manual
	// timer. The magnitude is SetByCaller (Ash.Data.StunDuration) so the duration comes from data.
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat StunDurationSBC;
	StunDurationSBC.DataTag = AshGameplayTags::Data_StunDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(StunDurationSBC);

	// Grant Ash.State.Stunned for the effect's lifetime (UE5.8 GameplayEffectComponent API).
	UTargetTagsGameplayEffectComponent& TagsComponent = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();
	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(AshGameplayTags::State_Stunned);
	TagsComponent.SetAndApplyTargetTagChanges(GrantedTags);
}
