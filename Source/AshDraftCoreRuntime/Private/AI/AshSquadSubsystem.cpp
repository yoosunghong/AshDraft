// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshSquadSubsystem.h"

FAshSquadState& UAshSquadSubsystem::RegisterSquad(int32 SquadId, EAshTeamId Team)
{
	FAshSquadState& State = Squads.FindOrAdd(SquadId);
	State.SquadId = SquadId;
	State.TeamId = Team;
	return State;
}

bool UAshSquadSubsystem::HasSquad(int32 SquadId) const
{
	return Squads.Contains(SquadId);
}

void UAshSquadSubsystem::SetSquadOrder(int32 SquadId, EAshSquadOrder Order, const FVector& ObjectiveLocation)
{
	FAshSquadState& State = Squads.FindOrAdd(SquadId);
	State.SquadId = SquadId;
	State.Order = Order;
	State.ObjectiveLocation = ObjectiveLocation;
	State.bHasObjective = true;
}

void UAshSquadSubsystem::SetSquadObjective(int32 SquadId, EAshSquadOrder Order, const FVector& ObjectiveLocation, float FormationRadius)
{
	FAshSquadState& State = Squads.FindOrAdd(SquadId);
	State.SquadId = SquadId;
	State.Order = Order;
	State.ObjectiveLocation = ObjectiveLocation;
	State.bHasObjective = true;
	State.FormationRadius = FormationRadius;
}

bool UAshSquadSubsystem::GetSquadObjective(int32 SquadId, FVector& OutObjective) const
{
	if (const FAshSquadState* State = Squads.Find(SquadId))
	{
		if (State->bHasObjective)
		{
			OutObjective = State->ObjectiveLocation;
			return true;
		}
	}
	return false;
}

bool UAshSquadSubsystem::GetSquadObjective(int32 SquadId, FVector& OutObjective, float& OutFormationRadius) const
{
	if (const FAshSquadState* State = Squads.Find(SquadId))
	{
		if (State->bHasObjective)
		{
			OutObjective = State->ObjectiveLocation;
			OutFormationRadius = State->FormationRadius;
			return true;
		}
	}
	return false;
}

FAshSquadState UAshSquadSubsystem::GetSquadState(int32 SquadId) const
{
	if (const FAshSquadState* State = Squads.Find(SquadId))
	{
		return *State;
	}
	return FAshSquadState();
}

void UAshSquadSubsystem::UpdateSquadAggregate(int32 SquadId, const FVector& AveragePosition, int32 AliveCount)
{
	// Only update existing squads; aggregation should never invent a squad the commander
	// doesn't know about.
	if (FAshSquadState* State = Squads.Find(SquadId))
	{
		State->AveragePosition = AveragePosition;
		State->AliveCount = AliveCount;
	}
}

TArray<int32> UAshSquadSubsystem::GetAllSquadIds() const
{
	TArray<int32> Ids;
	Squads.GetKeys(Ids);
	return Ids;
}

TArray<int32> UAshSquadSubsystem::GetSquadIdsForTeam(EAshTeamId Team) const
{
	TArray<int32> Ids;
	for (const TPair<int32, FAshSquadState>& Pair : Squads)
	{
		if (Pair.Value.TeamId == Team)
		{
			Ids.Add(Pair.Key);
		}
	}
	return Ids;
}
