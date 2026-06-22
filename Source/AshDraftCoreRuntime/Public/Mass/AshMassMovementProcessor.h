// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassMovementProcessor.generated.h"

/**
 * How a soldier steers toward a *group* objective (a squad objective or target base).
 * Combat-target chasing is always direct regardless of this mode.
 */
UENUM()
enum class EAshGroupNavMode : uint8
{
	/** Steer straight at the objective. Cheapest; the Phase 10 baseline to compare against. */
	Direct,

	/** Follow the shared flow field baked toward the objective (Phase 14, ARCHITECTURE.md 12.1). */
	FlowField,
};

/**
 * UAshMassMovementProcessor
 *
 * Phase 10 data-oriented movement (ARCHITECTURE.md 7.3 / 12). Every frame it steers each
 * soldier entity and integrates its position from velocity. Steering priority:
 *   1. A valid combat target (FAshCombatFragment::Target) -> move toward that entity (always direct).
 *   2. The squad's shared objective (UAshSquadSubsystem) -> advance on the objective.
 *   3. Fallback: the nearest non-friendly base (UAshBaseSubsystem) -> so the army has
 *      somewhere to go even before the commander assigns squad orders (Phase 11).
 * When already within attack range of the target the soldier stops so the combat
 * processor can strike (no overlap-piling beyond range; finer slot spacing is the
 * combat-slot system's job, ARCHITECTURE.md 10).
 *
 * Group-objective steering (cases 2/3) obeys GroupNavMode: Direct (straight line) or
 * FlowField (Phase 14), which reads one shared direction map from UAshFlowFieldSubsystem
 * so soldiers route as a group toward the objective base instead of each pathfinding
 * (ARCHITECTURE.md 12.1). Flip the mode to compare the two — it is config-backed so no
 * rebuild is needed.
 *
 * Integration runs every frame because it is cheap; the *expensive* decisions
 * (target acquisition) are throttled in the combat processor via AI LOD (Phase 12).
 * No per-soldier Actor, Tick, or pathfinding (ARCHITECTURE.md 7.4 / 18.3).
 */
// config=Game persists the EditDefaultsOnly tunables below to DefaultGame.ini under
// [/Script/AshDraftCoreRuntime.AshMassMovementProcessor] so GroupNavMode survives editor restarts.
UCLASS(config = Game)
class ASHDRAFTCORERUNTIME_API UAshMassMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Distance (cm) at which a soldier is considered "arrived" and stops advancing. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement", meta = (ClampMin = "0.0"))
	float ArrivalTolerance = 100.f;

	/** Direct vs flow-field steering for group objectives (Phase 14 comparison switch). */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement")
	EAshGroupNavMode GroupNavMode = EAshGroupNavMode::FlowField;

	/** When true (and in FlowField mode) draw the active flow field as per-cell arrows. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement|Debug")
	bool bDrawFlowFieldDebug = false;

private:
	FMassEntityQuery EntityQuery;
};
