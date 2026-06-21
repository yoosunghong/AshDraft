// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "AshGameplayAbility.generated.h"

class UAnimMontage;

/**
 * UAshGameplayAbility
 *
 * Base Gameplay Ability for AshDraft (ARCHITECTURE.md 5.4). Adds an InputTag the
 * pawn uses to bind the ability to an Enhanced Input action (data-driven,
 * Lyra-style) and defaults the instancing policy so derived abilities behave
 * predictably for single-player melee combat.
 */
UCLASS(Abstract)
class ASHDRAFTCORERUNTIME_API UAshGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UAshGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Input tag that activates this ability when the matching Enhanced Input action fires. */
	const FGameplayTag& GetInputTag() const { return InputTag; }

	/** Animation montage this ability plays on activation, if any. */
	UAnimMontage* GetAbilityMontage() const { return AbilityMontage; }

protected:
	/** Native input tag (e.g. Ash.InputTag.Attack.Basic) bound by the owning pawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Input", meta = (Categories = "Ash.InputTag"))
	FGameplayTag InputTag;

	/**
	 * Montage played when this ability activates (the per-ability animation slot).
	 * This is the canonical place to assign an ability's animation: set it on a
	 * GA_* Blueprint child of the ability so it stays data-driven (ARCHITECTURE 17).
	 * Optional — abilities still function without one.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Animation")
	TObjectPtr<UAnimMontage> AbilityMontage;

	/** Play rate applied to AbilityMontage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Animation", meta = (ClampMin = "0.1"))
	float AbilityMontagePlayRate = 1.f;
};
