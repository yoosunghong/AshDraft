// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "AshBTService_DetectPlayer.generated.h"

/**
 * UAshBTService_DetectPlayer
 *
 * Squad/general-level detection (ARCHITECTURE.md 8.1/8.3). Periodically locates the
 * local player pawn and, when within sight range, writes it to the blackboard target
 * key (the inherited BlackboardKey) along with the cached distance, and points the
 * controller's focus at it so the general faces the player before attacking. Uses
 * hysteresis (a larger lose-sight radius) so the target does not flicker at the edge.
 *
 * PoC scope: single local player via UGameplayStatics::GetPlayerPawn. A perception /
 * team-query based detector replaces this when multiple targets exist.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshBTService_DetectPlayer : public UBTService_BlackboardBase
{
	GENERATED_BODY()

public:
	UAshBTService_DetectPlayer();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Blackboard key (float) that receives the distance to the target. */
	UPROPERTY(EditAnywhere, Category = "Ash|AI")
	FBlackboardKeySelector TargetDistanceKey;

	/** Acquire the player when within this distance (cm). */
	UPROPERTY(EditAnywhere, Category = "Ash|AI", meta = (ClampMin = "0.0"))
	float SightRadius = 2000.f;

	/** Drop the player once beyond this distance (cm). Should be >= SightRadius (hysteresis). */
	UPROPERTY(EditAnywhere, Category = "Ash|AI", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 2500.f;
};
