// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshBTService_DetectPlayer.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UAshBTService_DetectPlayer::UAshBTService_DetectPlayer()
{
	NodeName = TEXT("Ash Detect Player");

	// Re-run a few times a second; detection does not need per-frame precision.
	Interval = 0.3f;
	RandomDeviation = 0.05f;

	// The inherited BlackboardKey is the target-actor key; accept object/actor types.
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UAshBTService_DetectPlayer, BlackboardKey), AActor::StaticClass());
	TargetDistanceKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UAshBTService_DetectPlayer, TargetDistanceKey));
}

void UAshBTService_DetectPlayer::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetDistanceKey.ResolveSelectedKey(*BBAsset);
	}
}

void UAshBTService_DetectPlayer::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	APawn* SelfPawn = AIController ? AIController->GetPawn() : nullptr;
	if (!AIController || !Blackboard || !SelfPawn)
	{
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(SelfPawn, 0);
	const bool bHadTarget = Blackboard->GetValueAsObject(BlackboardKey.SelectedKeyName) != nullptr;

	if (Player)
	{
		const float Distance = FVector::Dist(SelfPawn->GetActorLocation(), Player->GetActorLocation());

		// Hysteresis: acquire inside SightRadius, keep until beyond LoseSightRadius.
		const float Threshold = bHadTarget ? LoseSightRadius : SightRadius;
		if (Distance <= Threshold)
		{
			Blackboard->SetValueAsObject(BlackboardKey.SelectedKeyName, Player);
			Blackboard->SetValueAsFloat(TargetDistanceKey.SelectedKeyName, Distance);
			AIController->SetFocus(Player);
			return;
		}
	}

	// No valid target in range: clear so the BT falls back to idle.
	Blackboard->ClearValue(BlackboardKey.SelectedKeyName);
	AIController->ClearFocus(EAIFocusPriority::Gameplay);
}
