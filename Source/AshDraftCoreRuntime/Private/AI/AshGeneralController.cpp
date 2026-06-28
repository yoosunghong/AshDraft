// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshGeneralController.h"

#include "Character/AshGeneralCharacter.h"
#include "Components/StateTreeAIComponent.h"

AAshGeneralController::AAshGeneralController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Don't auto-start: the general assigns its config's StateTree after possession, then we run it.
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAIComponent"));
	StateTreeComponent->SetStartLogicAutomatically(false);
}

void AAshGeneralController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// The general drives start-up (it knows its config StateTree) via RunGeneralStateTree.
	if (AAshGeneralCharacter* General = Cast<AAshGeneralCharacter>(InPawn))
	{
		General->StartOperationalAI();
	}
}

void AAshGeneralController::OnUnPossess()
{
	StopGeneralStateTree();
	Super::OnUnPossess();
}

void AAshGeneralController::RunGeneralStateTree(UStateTree* StateTree)
{
	if (!StateTreeComponent || !StateTree)
	{
		return;
	}

	StateTreeComponent->SetStateTree(StateTree);
	StateTreeComponent->StartLogic();
}

void AAshGeneralController::StopGeneralStateTree()
{
	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("General stopped"));
	}
}

void AAshGeneralController::SetThinkInterval(float ThinkInterval)
{
	if (StateTreeComponent)
	{
		// 0 -> tick every frame; >0 -> tick at that cadence (actor AI-LOD throttle).
		StateTreeComponent->SetComponentTickInterval(FMath::Max(0.f, ThinkInterval));
	}
}
