// Copyright YoosungHong. All Rights Reserved.

#include "Input/AshInputConfig.h"

#include "InputAction.h"

UAshInputConfig::UAshInputConfig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const UInputAction* UAshInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FAshInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("UAshInputConfig: No native InputAction for tag [%s] on [%s]."),
			*InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

const UInputAction* UAshInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FAshInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("UAshInputConfig: No ability InputAction for tag [%s] on [%s]."),
			*InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}
