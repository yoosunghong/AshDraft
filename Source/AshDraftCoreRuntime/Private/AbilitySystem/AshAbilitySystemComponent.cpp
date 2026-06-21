// Copyright YoosungHong. All Rights Reserved.

#include "AbilitySystem/AshAbilitySystemComponent.h"

#include "AshGameplayTags.h"

void UAshAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// Decide first, act after — sending a gameplay event can re-enter ability
	// activation and mutate the activatable list mid-iteration.
	bool bMatchingAbilityActive = false;
	TArray<FGameplayAbilitySpecHandle, TInlineAllocator<4>> HandlesToActivate;

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			if (Spec.IsActive())
			{
				bMatchingAbilityActive = true;
			}
			else
			{
				HandlesToActivate.Add(Spec.Handle);
			}
		}
	}

	if (bMatchingAbilityActive)
	{
		// The ability is mid-combo: forward the press as a combo-advance event so
		// the active ability buffers / chains its next montage (input buffering).
		FGameplayEventData Payload;
		Payload.EventTag = AshGameplayTags::Event_Combo_Next;
		Payload.Instigator = GetAvatarActor();
		HandleGameplayEvent(AshGameplayTags::Event_Combo_Next, &Payload);
	}
	else
	{
		for (const FGameplayAbilitySpecHandle& Handle : HandlesToActivate)
		{
			TryActivateAbility(Handle);
		}
	}
}

void UAshAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			// Notify instanced abilities that the input was released (for charged/held abilities).
			if (Spec.Ability)
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, Spec.ActivationInfo.GetActivationPredictionKey());
			}
		}
	}
}
