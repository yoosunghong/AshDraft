// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "BehaviorTree/BTTaskNode.h"
#include "GameplayTagContainer.h"
#include "AshBTTask_ActivateAbilityByInputTag.generated.h"

/**
 * UAshBTTask_ActivateAbilityByInputTag
 *
 * Bridges the Behavior Tree to GAS: activates the controlled pawn's ability bound to
 * InputTag through the same input path the player uses
 * (UAshAbilitySystemComponent::AbilityInputTagPressed). Lets the enemy general reuse
 * the shared UAshGA_BasicAttack without a separate AI-only ability.
 *
 * Returns Succeeded once the activation request is sent (the melee sweep is instant).
 * Attack pacing is left to the Behavior Tree (a Wait node / Cooldown decorator), so
 * this task stays a thin, reusable trigger.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshBTTask_ActivateAbilityByInputTag : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UAshBTTask_ActivateAbilityByInputTag();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	/** Input tag of the ability to activate (default: Ash.InputTag.Attack.Basic). */
	UPROPERTY(EditAnywhere, Category = "Ash|AI", meta = (Categories = "Ash.InputTag"))
	FGameplayTag InputTag;
};
