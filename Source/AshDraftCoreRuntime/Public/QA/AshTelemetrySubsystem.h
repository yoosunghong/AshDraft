// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "QA/AshQABotTypes.h"
#include "Teams/AshTeamTypes.h"
#include "AshTelemetrySubsystem.generated.h"

class AAshBaseActor;

/**
 * UAshTelemetrySubsystem
 *
 * The Phase 17 telemetry + QA-bot interface (ARCHITECTURE.md 14). One world-scoped hub that:
 *
 *   - Builds an FAshObservation snapshot of the world (14.1) on demand — the GetObservation()
 *     half of the future Reset()/Step()/GetObservation() RL contract (14.3).
 *   - Applies an FAshAction back to the player hero (14.2) — the action-input abstraction /
 *     Step() half — by translating the flat action into movement + ability-tag presses.
 *   - Records the combat log, base-event log, and match-result log (14.4) as capped ring
 *     buffers, auto-subscribing to base ownership changes and match end so most logging needs
 *     no explicit calls.
 *   - Exports the observation + logs as JSON (to a string or a file under Saved/AshTelemetry)
 *     so an external Python/RL harness can consume them.
 *
 * It only *observes* gameplay; it never drives AI or owns UI (ARCHITECTURE.md 16 / 18.4), and
 * holds no hard references to units — it resolves them through the existing subsystems and the
 * player controller each call. The combat-damage choke point (UAshAttributeSet) forwards hits
 * here via the static Get() accessor, the one inbound dependency.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshTelemetrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~UWorldSubsystem interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~End of UWorldSubsystem interface

	/** Convenience accessor for non-subsystem callers (e.g. the GAS damage path). May be null. */
	static UAshTelemetrySubsystem* Get(const UObject* WorldContextObject);

	// --- Observation / Action (ARCHITECTURE.md 14.1-14.3) ---

	/** Snapshots the current world state into an observation (the GetObservation() seam). */
	UFUNCTION(BlueprintCallable, Category = "Ash|QA")
	FAshObservation BuildObservation() const;

	/** Applies an action to the player hero (the Step(Action) seam). Returns true if applied. */
	UFUNCTION(BlueprintCallable, Category = "Ash|QA")
	bool ApplyAction(const FAshAction& Action);

	// --- Logging (ARCHITECTURE.md 14.4) ---

	/** Appends a combat/damage event. Called from the GAS damage choke point and Blueprints. */
	UFUNCTION(BlueprintCallable, Category = "Ash|QA")
	void LogCombatEvent(const FString& Source, const FString& Target, float Amount, FName EventType);

	/** Copy of the recorded combat events. */
	UFUNCTION(BlueprintPure, Category = "Ash|QA")
	TArray<FAshCombatLogEntry> GetCombatLog() const { return CombatLog; }

	/** Copy of the recorded base-ownership events. */
	UFUNCTION(BlueprintPure, Category = "Ash|QA")
	TArray<FAshBaseEventLogEntry> GetBaseEventLog() const { return BaseEventLog; }

	/** Copy of the recorded match results (one per finished match this session). */
	UFUNCTION(BlueprintPure, Category = "Ash|QA")
	TArray<FAshMatchResultLogEntry> GetMatchResultLog() const { return MatchResultLog; }

	// --- Export (ARCHITECTURE.md 14 / Phase 17 "export as JSON") ---

	/** Serializes the current observation only to a JSON string. */
	UFUNCTION(BlueprintCallable, Category = "Ash|QA")
	FString GetObservationJson() const;

	/** Serializes the observation + all logs to a JSON string. */
	UFUNCTION(BlueprintCallable, Category = "Ash|QA")
	FString ExportToJson() const;

	/**
	 * Writes ExportToJson() to disk. With an empty path, writes a timestamped file under
	 * <ProjectSaved>/AshTelemetry/. Returns the absolute path written, or empty on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ash|QA")
	FString ExportToFile(const FString& OptionalAbsolutePath);

private:
	/** Bound to the base subsystem: records base flips into the base-event log. */
	UFUNCTION()
	void HandleBaseOwnershipChanged(AAshBaseActor* Base, EAshTeamId OldTeam, EAshTeamId NewTeam);

	/** Bound to the match subsystem: records the terminal result into the match-result log. */
	UFUNCTION()
	void HandleMatchEnded(EAshMatchResult Result, EAshMatchEndReason Reason);

	/** Resolves the player hero pawn (cast helper), or nullptr. */
	class AAshHeroCharacter* GetPlayerHero() const;

	/** Derives the human-readable current objective string from base ownership vs the player. */
	FString ComputeCurrentObjective(const FVector& FromLocation) const;

	/** Counts living enemy-side actor units within ObservationRadius of FromLocation. */
	int32 CountNearbyEnemies(const FVector& FromLocation) const;

	/** Pushes Entry into Buffer, trimming the oldest beyond MaxLogEntries. */
	template <typename T>
	void AppendCapped(TArray<T>& Buffer, const T& Entry);

	/** Current world time (s), safe against a null world. */
	float NowSeconds() const;

	/** Radius (cm) for the "nearby enemies" observation. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|QA", meta = (ClampMin = "0.0"))
	float ObservationRadius = 3000.f;

	/** Per-log cap; oldest entries are dropped past this to keep memory bounded. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|QA", meta = (ClampMin = "1"))
	int32 MaxLogEntries = 4096;

	UPROPERTY(Transient)
	TArray<FAshCombatLogEntry> CombatLog;

	UPROPERTY(Transient)
	TArray<FAshBaseEventLogEntry> BaseEventLog;

	UPROPERTY(Transient)
	TArray<FAshMatchResultLogEntry> MatchResultLog;

	bool bBoundToBases = false;
	bool bBoundToMatch = false;
};
