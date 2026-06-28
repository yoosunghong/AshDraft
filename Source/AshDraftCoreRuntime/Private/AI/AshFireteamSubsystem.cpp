// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshFireteamSubsystem.h"

void UAshFireteamSubsystem::BeginFireteamUpdate()
{
	Fireteams.Reset();
}

void UAshFireteamSubsystem::UpsertFireteam(const FAshFireteamState& State)
{
	Fireteams.Add(State.FireteamId, State);
}

void UAshFireteamSubsystem::SetAssignedEnemyFireteam(int32 FireteamId, int32 EnemyFireteamId)
{
	if (FAshFireteamState* State = Fireteams.Find(FireteamId))
	{
		State->AssignedEnemyFireteamId = EnemyFireteamId;
	}
}

bool UAshFireteamSubsystem::GetFireteamState(int32 FireteamId, FAshFireteamState& OutState) const
{
	if (const FAshFireteamState* State = Fireteams.Find(FireteamId))
	{
		OutState = *State;
		return true;
	}
	return false;
}

TArray<FAshFireteamState> UAshFireteamSubsystem::GetAllFireteamStates() const
{
	TArray<FAshFireteamState> States;
	Fireteams.GenerateValueArray(States);
	return States;
}
