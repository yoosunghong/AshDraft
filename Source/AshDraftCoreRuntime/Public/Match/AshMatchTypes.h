// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AshMatchTypes.generated.h"

/**
 * AshMatchTypes.h
 *
 * Shared data definitions for the match-flow layer (PLAN Phase 16, ARCHITECTURE.md 15/16).
 * Trivial enums grouped in one header because they are plain data shared by the match
 * subsystem and any UI that observes it. Behavior lives in UAshMatchSubsystem, never here.
 */

/**
 * Lifecycle of a single PoC match. The match subsystem owns exactly one of these per world
 * and transitions NotStarted -> InProgress -> (Victory | Defeat), firing events on each edge
 * so UI and telemetry can react without polling.
 */
UENUM(BlueprintType)
enum class EAshMatchState : uint8
{
	/** World loaded but the match loop has not begun (pre-BeginPlay / set-up). */
	NotStarted	= 0	UMETA(DisplayName = "Not Started"),

	/** The match is live; win/lose conditions are being evaluated. */
	InProgress	= 1	UMETA(DisplayName = "In Progress"),

	/** The player side won (terminal). */
	Victory		= 2	UMETA(DisplayName = "Victory"),

	/** The player side lost (terminal). */
	Defeat		= 3	UMETA(DisplayName = "Defeat")
};

/** Outcome of a finished match; None while it is still running. */
UENUM(BlueprintType)
enum class EAshMatchResult : uint8
{
	None	= 0	UMETA(DisplayName = "None"),
	Victory	= 1	UMETA(DisplayName = "Victory"),
	Defeat	= 2	UMETA(DisplayName = "Defeat")
};

/**
 * Why a match ended. Kept distinct from the result so telemetry / the result UI can explain
 * the outcome ("Defeat — main base lost" vs "Defeat — you died"). Maps to the Phase 16 tasks:
 * all bases captured (victory), player death (defeat), ally main base captured (defeat).
 */
UENUM(BlueprintType)
enum class EAshMatchEndReason : uint8
{
	/** Still running. */
	None				= 0	UMETA(DisplayName = "None"),

	/** Player side owns every base (Phase 16 victory condition). */
	AllBasesCaptured	= 1	UMETA(DisplayName = "All Bases Captured"),

	/** The playable hero died (Phase 16 defeat condition). */
	PlayerDeath			= 2	UMETA(DisplayName = "Player Death"),

	/** A player-side main base was captured by the enemy (Phase 16 defeat condition). */
	MainBaseLost		= 3	UMETA(DisplayName = "Main Base Lost")
};
