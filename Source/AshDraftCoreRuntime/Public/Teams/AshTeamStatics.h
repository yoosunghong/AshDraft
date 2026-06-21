// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Teams/AshTeamTypes.h"
#include "AshTeamStatics.generated.h"

/**
 * UAshTeamStatics
 *
 * Team helper functions shared across AshDraft (ARCHITECTURE.md 4.4 / 18.4). Centralizes
 * "what team is this actor on?" and "are these two teams hostile?" so combat slots,
 * bases, and AI all classify units the same way.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshTeamStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Resolves an actor's team via IAshTeamAgentInterface, checking the actor first and
	 * then its controller (for AI/player pawns). Returns Neutral when no team is found.
	 */
	UFUNCTION(BlueprintPure, Category = "Ash|Team")
	static EAshTeamId GetActorTeam(const AActor* Actor);

	/**
	 * True when two teams should fight. Player and Ally are mutually friendly; Enemy is
	 * hostile to Player/Ally; Neutral is hostile to no one (and no one is hostile to it).
	 */
	UFUNCTION(BlueprintPure, Category = "Ash|Team")
	static bool AreTeamsHostile(EAshTeamId TeamA, EAshTeamId TeamB);
};
