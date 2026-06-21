// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshEnemyGeneralController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

const FName AAshEnemyGeneralController::TargetActorKeyName(TEXT("TargetActor"));
const FName AAshEnemyGeneralController::TargetDistanceKeyName(TEXT("TargetDistance"));

AAshEnemyGeneralController::AAshEnemyGeneralController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// AI logic is BT-driven; no controller Tick needed (ARCHITECTURE.md 18.3).
	PrimaryActorTick.bCanEverTick = false;
}

void AAshEnemyGeneralController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BehaviorTree && BehaviorTree->BlackboardAsset)
	{
		UBlackboardComponent* BlackboardComp = nullptr;
		UseBlackboard(BehaviorTree->BlackboardAsset, BlackboardComp);
		RunBehaviorTree(BehaviorTree);
	}
}

void AAshEnemyGeneralController::OnUnPossess()
{
	StopBehavior();
	Super::OnUnPossess();
}

void AAshEnemyGeneralController::StopBehavior()
{
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTComp->StopTree();
	}
}
