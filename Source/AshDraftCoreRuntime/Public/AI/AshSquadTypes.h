// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Teams/AshTeamTypes.h"
#include "AshSquadTypes.generated.h"

/**
 * AshSquadTypes.h
 *
 * Shared data definitions for the hierarchical AI layer (ARCHITECTURE.md 8). These are
 * trivial POD-style definitions (an order enum + a squad state struct) grouped in one
 * header because they are plain data shared by the squad subsystem, the commander
 * subsystem, and the Mass processors that read squad objectives. Behavior lives in the
 * subsystems/processors, never here.
 */

/**
 * Strategic order assigned to a squad by the Commander AI (ARCHITECTURE.md 8.1).
 * Mirrors the doc's "Order.*" vocabulary but kept as a cheap enum the Mass movement
 * processor can branch on without touching Gameplay Tags per entity.
 */
UENUM(BlueprintType)
enum class EAshSquadOrder : uint8
{
	/** No order yet; soldiers hold position. */
	None		= 0	UMETA(DisplayName = "None"),

	/** Advance on and capture the objective base (ARCHITECTURE.md 8.1 Order.AttackBase). */
	AttackBase	= 1	UMETA(DisplayName = "Attack Base"),

	/** Hold and protect the objective base (Order.DefendBase). */
	DefendBase	= 2	UMETA(DisplayName = "Defend Base"),

	/** Fall back toward the rally/objective location (Order.Retreat). */
	Retreat		= 3	UMETA(DisplayName = "Retreat")
};

/**
 * Squad-level state tracked by UAshSquadSubsystem (ARCHITECTURE.md 8.2).
 *
 * A squad is a logical group of Mass soldier entities sharing a SquadId. Rather than
 * each soldier running strategic logic, the squad holds one objective and aggregate
 * stats (average position, alive count) that the commander reasons over and the
 * movement processor steers toward. This is the "shared target" the data-oriented
 * rules call for (ARCHITECTURE.md 7.4).
 */
USTRUCT(BlueprintType)
struct FAshSquadState
{
	GENERATED_BODY()

	/** Unique squad id; matches FAshSquadFragment::SquadId on member entities. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	int32 SquadId = INDEX_NONE;

	/** Team this squad fights for. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	EAshTeamId TeamId = EAshTeamId::Neutral;

	/** Current strategic order from the commander. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	EAshSquadOrder Order = EAshSquadOrder::None;

	/** World-space objective the squad is moving to (e.g. the target base location). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	FVector ObjectiveLocation = FVector::ZeroVector;

	/** True once ObjectiveLocation has been assigned (distinguishes from a real zero objective). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	bool bHasObjective = false;

	/**
	 * Radius (cm) around ObjectiveLocation that members treat as "arrived" / form up within (Phase 22).
	 * A General publishes its own live position as the objective with this radius, so its troops settle
	 * into a ring around it instead of all ramming the exact point. 0 = use the movement processor's
	 * default arrival tolerance (legacy commander-driven squads leave this 0).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	float FormationRadius = 0.f;

	/**
	 * Stable world-space facing the squad forms up along (Phase 27). A General publishes its own forward
	 * vector here. The fireteam formation uses this fixed orientation instead of deriving it from each
	 * fireteam's instantaneous position relative to the objective — which rotated as the fireteam moved,
	 * so the formation slot rotated and the troops orbited the objective forever instead of settling.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	FVector ObjectiveFacing = FVector::ForwardVector;

	/** Mean position of living members, refreshed by UAshMassSquadTrackingProcessor. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	FVector AveragePosition = FVector::ZeroVector;

	/** Count of living members from the last aggregation pass. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Squad")
	int32 AliveCount = 0;
};
