// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AshAbilitySystemComponent.generated.h"

/**
 * UAshAbilitySystemComponent
 *
 * AshDraft's AbilitySystemComponent wrapper (ARCHITECTURE.md 5). Thin for the
 * Phase 4 foundation; it exists so all Ash GAS owners share one type and so
 * project-wide input routing and ability policy can be added here later without
 * touching every avatar.
 *
 * Input routing: gameplay abilities granted by AAshHeroCharacter carry an input
 * GameplayTag in their dynamic spec tags. The owning pawn forwards Enhanced
 * Input presses/releases here by tag, and this component activates / ends the
 * matching abilities — the Lyra-style data-driven input-to-ability path.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** Activates every activatable ability whose spec dynamic tags contain InputTag. */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/** Ends abilities bound to InputTag (for held/charged abilities). */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
};
