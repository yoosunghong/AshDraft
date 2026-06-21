// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AshInputConfig.generated.h"

class UInputAction;

/**
 * Associates a single Enhanced Input Action with a native input GameplayTag.
 * The tag is how gameplay code / abilities reference the action without a hard
 * pointer to the input asset.
 */
USTRUCT(BlueprintType)
struct FAshInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Ash.InputTag"))
	FGameplayTag InputTag;
};

/**
 * Data-driven mapping of Input Actions to input tags (Lyra-style).
 *
 * NativeInputActions are bound directly to handler functions (e.g. Move, Look).
 * AbilityInputActions are routed to GAS by their InputTag once the ability
 * system exists in a later phase.
 *
 * Authored as a Data Asset under Content/Data/Input/ and referenced by the
 * pawn / player controller setup in Phase 2-3.
 */
UCLASS(BlueprintType, Const)
class ASHDRAFTCORERUNTIME_API UAshInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UAshInputConfig(const FObjectInitializer& ObjectInitializer);

	/** Returns the Input Action bound to a native input tag, or nullptr. */
	UFUNCTION(BlueprintCallable, Category = "Ash|Input", meta = (ReturnDisplayName = "InputAction"))
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** Returns the Input Action bound to an ability input tag, or nullptr. */
	UFUNCTION(BlueprintCallable, Category = "Ash|Input", meta = (ReturnDisplayName = "InputAction"))
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** Input Actions bound directly to native handlers (movement, look, etc.). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Input", meta = (TitleProperty = "InputTag"))
	TArray<FAshInputAction> NativeInputActions;

	/** Input Actions that activate gameplay abilities by tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Input", meta = (TitleProperty = "InputTag"))
	TArray<FAshInputAction> AbilityInputActions;
};
