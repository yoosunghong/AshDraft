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

	/**
	 * Personal-space radius (cm) used for inter-soldier avoidance. Neighbours closer than this push
	 * a soldier away so the army spreads out instead of stacking on one point. ~ the soldier's body
	 * diameter. Set to 0 to disable avoidance entirely.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement|Avoidance", meta = (ClampMin = "0.0"))
	float SeparationRadius = 90.f;

	/**
	 * Strength of the avoidance push relative to MoveSpeed. 1.0 lets crowding fully cancel forward
	 * speed at the radius; lower values keep soldiers pressing toward the objective while still
	 * spreading. The combined steer+separation velocity is clamped to MoveSpeed.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement|Avoidance", meta = (ClampMin = "0.0"))
	float SeparationStrength = 0.6f;

	/**
	 * Fallback per-frame relaxation factor (0..1) for soldiers without a behavior config. < 1 makes a
	 * crowd settle into stable spacing instead of oscillating (the cure for the "5+ shake" jitter).
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement|Avoidance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SeparationRelaxation = 0.5f;

	/** Fallback cap on a single neighbour's push so two stacked soldiers can't fling each other. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement|Avoidance", meta = (ClampMin = "0.0"))
	float MaxPushPerNeighbor = 1.f;

	/** Fallback resistance (0..1) an attacking soldier has to being pushed; holds the line in melee. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement|Avoidance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CombatAnchorResistance = 0.85f;

	/** Fallback body turn rate (deg/s) for target-aware interpolated facing. */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement|Facing", meta = (ClampMin = "0.0"))
	float FacingTurnRateDegPerSec = 720.f;

	/**
	 * Fallback ease-in distance (cm) to a group objective for soldiers without a behavior config.
	 * Within this range the steer speed scales with the remaining distance so soldiers settle onto a
	 * shared objective instead of ramming it at full speed and rebounding (the crowd-shake cure). 0
	 * disables the ease-in.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|MassMovement|Avoidance", meta = (ClampMin = "0.0"))
	float ArrivalSlowdownRadius = 250.f;

private:
	FMassEntityQuery EntityQuery;
};
