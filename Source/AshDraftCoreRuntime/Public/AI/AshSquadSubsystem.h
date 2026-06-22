// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "AI/AshSquadTypes.h"
#include "AshSquadSubsystem.generated.h"

/**
 * UAshSquadSubsystem
 *
 * World-scoped registry of squad state for the hierarchical AI (ARCHITECTURE.md 8.2 /
 * 18.4). It is the seam between the strategic layer (UAshCommanderSubsystem, which
 * assigns orders) and the data-oriented layer (the Mass movement processor, which reads
 * each squad's objective, and UAshMassSquadTrackingProcessor, which writes back the
 * aggregated average position / alive count).
 *
 * Squads are keyed by id and identified to entities through FAshSquadFragment::SquadId,
 * so soldiers never hold a hard reference to a squad object — they only carry an int id
 * and ask this subsystem for the shared objective (ARCHITECTURE.md 7.4 / 18.4).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshSquadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Creates (or returns) the squad state for SquadId on Team. Idempotent. */
	FAshSquadState& RegisterSquad(int32 SquadId, EAshTeamId Team);

	/** True if a squad with SquadId exists. */
	UFUNCTION(BlueprintPure, Category = "Ash|Squad")
	bool HasSquad(int32 SquadId) const;

	/** Assigns an order + objective location to a squad (called by the commander). */
	void SetSquadOrder(int32 SquadId, EAshSquadOrder Order, const FVector& ObjectiveLocation);

	/**
	 * Returns the squad's current objective location if it has one. Used by the Mass
	 * movement processor to steer member soldiers toward a shared goal.
	 */
	bool GetSquadObjective(int32 SquadId, FVector& OutObjective) const;

	/** Copy of a squad's full state (default-constructed if unknown). */
	UFUNCTION(BlueprintPure, Category = "Ash|Squad")
	FAshSquadState GetSquadState(int32 SquadId) const;

	/** Writes back the aggregate stats computed by UAshMassSquadTrackingProcessor. */
	void UpdateSquadAggregate(int32 SquadId, const FVector& AveragePosition, int32 AliveCount);

	/** All known squad ids (e.g. for the tracking processor / commander to iterate). */
	UFUNCTION(BlueprintPure, Category = "Ash|Squad")
	TArray<int32> GetAllSquadIds() const;

	/** Squads belonging to Team. */
	TArray<int32> GetSquadIdsForTeam(EAshTeamId Team) const;

private:
	/** SquadId -> state. The single source of truth for squad-level data this frame. */
	TMap<int32, FAshSquadState> Squads;
};
