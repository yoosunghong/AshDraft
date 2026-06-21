// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "AshEnemyGeneralController.generated.h"

class UBehaviorTree;

/**
 * AAshEnemyGeneralController
 *
 * AIController for the enemy general (ARCHITECTURE.md 6.2 / 8). On possession it
 * runs the assigned Behavior Tree, which drives detection, approach (NavMesh
 * MoveTo), and the GAS basic attack via the project's custom BT nodes
 * (UAshBTService_DetectPlayer, UAshBTTask_ActivateAbilityByInputTag).
 *
 * Keeps strategic decisions thin for this phase: the general reacts to the player
 * directly. Squad / commander coordination is layered on in later phases without
 * this controller depending on UI (ARCHITECTURE.md 16 / 18.4).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshEnemyGeneralController : public AAIController
{
	GENERATED_BODY()

public:
	AAshEnemyGeneralController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AController interface
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	//~End of AController interface

	/** Stops the running Behavior Tree (called on the general's death). */
	void StopBehavior();

	/** Blackboard key holding the current target actor (the player). */
	static const FName TargetActorKeyName;

	/** Blackboard key holding the cached distance to the target (cm). */
	static const FName TargetDistanceKeyName;

protected:
	/** Behavior Tree run on possession; assign a BT asset on a Blueprint child or instance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;
};
