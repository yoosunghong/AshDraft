// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Teams/AshTeamTypes.h"
#include "AshFireteamSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FAshFireteamState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ash|Fireteam")
	int32 FireteamId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|Fireteam")
	int32 SquadId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|Fireteam")
	int32 FireteamIndexInSquad = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|Fireteam")
	EAshTeamId TeamId = EAshTeamId::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|Fireteam")
	FVector AveragePosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|Fireteam")
	FVector LeaderPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|Fireteam")
	int32 AliveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|Fireteam")
	int32 AssignedEnemyFireteamId = INDEX_NONE;
};

/**
 * Frame-local registry for 5-soldier fireteams. The Mass fireteam processor refreshes it, while
 * debug/UI/higher AI can inspect the current decomposition without scanning Mass fragments again.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshFireteamSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void BeginFireteamUpdate();
	void UpsertFireteam(const FAshFireteamState& State);
	void SetAssignedEnemyFireteam(int32 FireteamId, int32 EnemyFireteamId);

	bool GetFireteamState(int32 FireteamId, FAshFireteamState& OutState) const;
	TArray<FAshFireteamState> GetAllFireteamStates() const;

private:
	TMap<int32, FAshFireteamState> Fireteams;
};
