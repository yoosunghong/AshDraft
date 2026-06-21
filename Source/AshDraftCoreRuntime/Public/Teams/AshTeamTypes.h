// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AshTeamTypes.generated.h"

/**
 * Battlefield team identity for AshDraft units (ARCHITECTURE.md 18.1).
 *
 * This is the lightweight, data-oriented team classification shared by heroes,
 * the enemy general, and (later) Mass soldiers. It mirrors the `Ash.Team.*`
 * Gameplay Tags but is cheap to store on fragments and compare in bulk.
 */
UENUM(BlueprintType)
enum class EAshTeamId : uint8
{
	/** Non-aligned / unowned. */
	Neutral	= 0	UMETA(DisplayName = "Neutral"),

	/** Locally controlled player team. */
	Player	= 1	UMETA(DisplayName = "Player"),

	/** Friendly AI allied with the player. */
	Ally	= 2	UMETA(DisplayName = "Ally"),

	/** Hostile AI team. */
	Enemy	= 3	UMETA(DisplayName = "Enemy")
};
