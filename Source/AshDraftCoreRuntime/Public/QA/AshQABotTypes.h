// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Match/AshMatchTypes.h"
#include "AshQABotTypes.generated.h"

/**
 * AshQABotTypes.h
 *
 * Data contracts for the future QA-bot / reinforcement-learning interface (PLAN Phase 17,
 * ARCHITECTURE.md 14). These are plain USTRUCTs with UPROPERTY fields so the telemetry
 * subsystem can serialize them to JSON with FJsonObjectConverter and a Python/RL agent can
 * round-trip them. No behavior lives here — gathering and applying happens in
 * UAshTelemetrySubsystem (ARCHITECTURE.md 14.3 Reset()/Step()/GetObservation()).
 *
 * The shapes intentionally mirror the JSON examples in ARCHITECTURE.md 14.1 / 14.2.
 */

/**
 * A single environment observation (ARCHITECTURE.md 14.1). Everything an agent needs to pick
 * an action this step: player vitals + pose, local threat density, the strategic base picture,
 * and the current objective string.
 */
USTRUCT(BlueprintType)
struct FAshObservation
{
	GENERATED_BODY()

	/** World time (s) the observation was taken. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float TimeSeconds = 0.f;

	/** Player hero current health. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float PlayerHealth = 0.f;

	/** Player hero maximum health (for normalization). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float PlayerMaxHealth = 0.f;

	/** True while the hero is alive and the observation is meaningful. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	bool bPlayerAlive = false;

	/** Player hero world position. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	FVector PlayerPosition = FVector::ZeroVector;

	/** Player control yaw (degrees) — the camera/facing the agent steers. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float PlayerYaw = 0.f;

	/** Living hostile units within the observation radius of the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	int32 NearbyEnemyCount = 0;

	/** Bases owned by the player side (Player + Ally). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	int32 OwnedBases = 0;

	/** Bases owned by the enemy. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	int32 EnemyBases = 0;

	/** Bases still neutral. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	int32 NeutralBases = 0;

	/** Human/agent-readable current objective (e.g. "Capture:B_BaseCenter"). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	FString CurrentObjective;

	/** Current match lifecycle state. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	EAshMatchState MatchState = EAshMatchState::NotStarted;
};

/**
 * A single action an agent applies for one step (ARCHITECTURE.md 14.2). Abstracts the
 * concrete Enhanced Input / GAS plumbing behind a flat struct the agent fills in.
 */
USTRUCT(BlueprintType)
struct FAshAction
{
	GENERATED_BODY()

	/** Desired 2D move axis (screen-relative), components in [-1, 1]. */
	UPROPERTY(BlueprintReadWrite, Category = "Ash|QA")
	FVector2D Move = FVector2D::ZeroVector;

	/** Desired camera yaw delta (degrees) this step. */
	UPROPERTY(BlueprintReadWrite, Category = "Ash|QA")
	float CameraYaw = 0.f;

	/** Request the basic attack. */
	UPROPERTY(BlueprintReadWrite, Category = "Ash|QA")
	bool bAttack = false;

	/** Request a dodge. */
	UPROPERTY(BlueprintReadWrite, Category = "Ash|QA")
	bool bDodge = false;

	/** Request skill slot 1 (heavy attack in the current PoC). */
	UPROPERTY(BlueprintReadWrite, Category = "Ash|QA")
	bool bSkill1 = false;
};

/** One combat/damage event for the combat log (ARCHITECTURE.md 14.4). */
USTRUCT(BlueprintType)
struct FAshCombatLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float TimeSeconds = 0.f;

	/** Attacker display name. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	FString Source;

	/** Victim display name. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	FString Target;

	/** Damage dealt. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float Amount = 0.f;

	/** Event classifier ("Damage", "Kill", ...). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	FName EventType;
};

/** One base-ownership change for the base-event log (ARCHITECTURE.md 14.4). */
USTRUCT(BlueprintType)
struct FAshBaseEventLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float TimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	FString BaseName;

	/** Previous owning team id (EAshTeamId as int). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	int32 OldTeam = 0;

	/** New owning team id (EAshTeamId as int). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	int32 NewTeam = 0;
};

/** The terminal match outcome for the match-result log (ARCHITECTURE.md 14.4). */
USTRUCT(BlueprintType)
struct FAshMatchResultLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float TimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	EAshMatchResult Result = EAshMatchResult::None;

	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	EAshMatchEndReason Reason = EAshMatchEndReason::None;

	/** Match duration (s). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|QA")
	float MatchDuration = 0.f;
};
