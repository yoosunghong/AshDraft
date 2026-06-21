// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshBTTask_ActivateAbilityByInputTag.h"

#include "AbilitySystem/AshAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "AshGameplayTags.h"
#include "GameFramework/Pawn.h"

UAshBTTask_ActivateAbilityByInputTag::UAshBTTask_ActivateAbilityByInputTag()
{
	NodeName = TEXT("Ash Activate Ability (Input Tag)");
	InputTag = AshGameplayTags::InputTag_Attack_Basic;
}

EBTNodeResult::Type UAshBTTask_ActivateAbilityByInputTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	if (!Pawn || !InputTag.IsValid())
	{
		return EBTNodeResult::Failed;
	}

	UAshAbilitySystemComponent* AshASC = Cast<UAshAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn));
	if (!AshASC)
	{
		return EBTNodeResult::Failed;
	}

	// Reuse the player's input-driven activation path so the AI shares one attack ability.
	AshASC->AbilityInputTagPressed(InputTag);
	return EBTNodeResult::Succeeded;
}
