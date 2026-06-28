// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "AshGeneralController.generated.h"

class UStateTreeAIComponent;
class UStateTree;

/**
 * AAshGeneralController
 *
 * AIController for the StateTree-driven General (Phase 22, ARCHITECTURE.md 6.2 / 8). It hosts a
 * UStateTreeAIComponent (GameplayStateTree) — the StateTree replaces the Phase 5 Behavior Tree as the
 * general's operational decision logic. On possession it points the component at the general's
 * configured StateTree asset and starts it.
 *
 * Actor AI-LOD is applied here: UAshGeneralSubsystem tells the general how often to "think", and the
 * general forwards that to SetThinkInterval, which throttles the StateTree component's tick (far
 * generals decide at ~1-2 Hz, near ones at ~30-60 Hz) without any per-frame Actor Tick on the general.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshGeneralController : public AAIController
{
	GENERATED_BODY()

public:
	AAshGeneralController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AController interface
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	//~End of AController interface

	/** Starts the StateTree logic with the given asset (called by the general after possession). */
	void RunGeneralStateTree(UStateTree* StateTree);

	/** Stops the StateTree (called on the general's death). */
	void StopGeneralStateTree();

	/** Throttles the StateTree component's tick to ThinkInterval seconds (actor AI-LOD). */
	void SetThinkInterval(float ThinkInterval);

protected:
	/** StateTree component that runs the general's operational logic. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;
};
