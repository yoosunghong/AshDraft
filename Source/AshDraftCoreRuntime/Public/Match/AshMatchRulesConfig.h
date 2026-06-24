// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Match/AshMatchTypes.h"
#include "AshMatchRulesConfig.generated.h"

/**
 * UAshMatchRulesConfig
 *
 * Data-driven win/lose conditions for a single PoC match (PLAN Phase 15, ARCHITECTURE.md
 * 15/16/17). Phase 16 hardcoded the conditions inside UAshMatchSubsystem; this asset lets a
 * designer pick which conditions are active per level in the editor instead. Drop an
 * AAshMatchRulesActor in the level and point it at one of these, and the match subsystem
 * gates each end check on the flags below.
 *
 * The conditions stay event-driven and GAS-backed: hero / general death already flow from the
 * GAS attribute set (UAshAttributeSet -> OnHeroDied / OnGeneralDied), base flips come from
 * UAshBaseSubsystem, and the time limit is a single world timer. This asset only chooses
 * *which* of those signals end the match and how.
 *
 * Defaults intentionally reproduce the Phase 16 behavior (all bases captured = victory; hero
 * death or main base lost = defeat) so a level with no rules asset plays exactly as before.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshMatchRulesConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// --- Victory conditions ---

	/** Victory when the player side (Player/Ally) owns every registered base. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Match|Victory")
	bool bVictoryWhenAllBasesCaptured = true;

	/** Victory when every enemy general in the level has died (ignored if there are none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Match|Victory")
	bool bVictoryWhenEnemyGeneralsEliminated = false;

	// --- Defeat conditions ---

	/** Defeat when the playable hero dies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Match|Defeat")
	bool bDefeatWhenHeroDies = true;

	/** Defeat when a player-side main base (AAshBaseActor::IsMainBase) is captured by the enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Match|Defeat")
	bool bDefeatWhenMainBaseLost = true;

	// --- Optional time limit ---

	/** Match time limit in seconds; 0 (or Outcome == None) disables the timer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Match|Time",
		meta = (ClampMin = "0.0", EditCondition = "TimeLimitOutcome != EAshTimeLimitOutcome::None"))
	float TimeLimitSeconds = 0.f;

	/** What the time limit expiring means (survive-to-win vs. time-out-to-lose). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Match|Time")
	EAshTimeLimitOutcome TimeLimitOutcome = EAshTimeLimitOutcome::None;
};
