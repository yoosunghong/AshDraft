// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "Match/AshMatchTypes.h"
#include "Teams/AshTeamTypes.h"
#include "AshMatchSubsystem.generated.h"

class AAshBaseActor;
class AAshHeroCharacter;

/** Match started: fired once when the loop begins (after all actors BeginPlay). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAshOnMatchStarted);

/** Match ended: terminal result + the reason it ended. UI / telemetry bind here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAshOnMatchEnded, EAshMatchResult, Result, EAshMatchEndReason, Reason);

/** Any state transition (NotStarted/InProgress/Victory/Defeat). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAshOnMatchStateChanged, EAshMatchState, NewState);

/**
 * UAshMatchSubsystem
 *
 * The PoC match loop (PLAN Phase 16, ARCHITECTURE.md 15/16). A world-scoped state machine
 * that begins the match on world BeginPlay and watches for the three end conditions the
 * phase calls for, then broadcasts a terminal result the UI observes:
 *
 *   - Victory: the player side (Player/Ally) owns *every* registered base.
 *   - Defeat:  the playable hero dies.
 *   - Defeat:  a player-side *main* base (AAshBaseActor::IsMainBase) is captured by the enemy.
 *
 * It reacts to events rather than polling (ARCHITECTURE.md 18.3): it binds once to
 * UAshBaseSubsystem::OnAnyBaseOwnershipChanged and to the hero's OnHeroDied delegate. UI
 * reads results through the BlueprintAssignable delegates below — the subsystem never owns
 * UI or AI (ARCHITECTURE.md 16 / 18.4). As a UMG-free fallback it also prints the result
 * on screen so the loop is verifiable in PIE before any widget exists.
 *
 * Authority note (ARCHITECTURE.md 5.5 / 15): match end is an authoritative, event-driven
 * decision (not a cosmetic local effect), keeping it ready for future server authority.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshMatchSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~UWorldSubsystem interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~End of UWorldSubsystem interface

	/** Begins the match (idempotent): sets InProgress, evaluates once, broadcasts OnMatchStarted. */
	UFUNCTION(BlueprintCallable, Category = "Ash|Match")
	void StartMatch();

	/** Forces the match to end with an explicit result/reason (idempotent once terminal). */
	UFUNCTION(BlueprintCallable, Category = "Ash|Match")
	void EndMatch(EAshMatchResult Result, EAshMatchEndReason Reason);

	/** Current match state. */
	UFUNCTION(BlueprintPure, Category = "Ash|Match")
	EAshMatchState GetMatchState() const { return MatchState; }

	/** Terminal result (None until the match ends). */
	UFUNCTION(BlueprintPure, Category = "Ash|Match")
	EAshMatchResult GetMatchResult() const { return MatchResult; }

	/** Why the match ended (None until it ends). */
	UFUNCTION(BlueprintPure, Category = "Ash|Match")
	EAshMatchEndReason GetEndReason() const { return EndReason; }

	/** True while the match is live. */
	UFUNCTION(BlueprintPure, Category = "Ash|Match")
	bool IsMatchInProgress() const { return MatchState == EAshMatchState::InProgress; }

	/** Seconds since StartMatch() (0 before start, frozen at the end time once terminal). */
	UFUNCTION(BlueprintPure, Category = "Ash|Match")
	float GetMatchElapsedSeconds() const;

	/** Fired once when the match begins. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Match")
	FAshOnMatchStarted OnMatchStarted;

	/** Fired once when the match ends; carries the result and reason. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Match")
	FAshOnMatchEnded OnMatchEnded;

	/** Fired on every state transition. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Match")
	FAshOnMatchStateChanged OnMatchStateChanged;

private:
	/** Bound to the base subsystem; re-evaluates win/lose on every base flip. */
	UFUNCTION()
	void HandleBaseOwnershipChanged(AAshBaseActor* Base, EAshTeamId OldTeam, EAshTeamId NewTeam);

	/** Bound to the hero; ends the match as a defeat when the player dies. */
	UFUNCTION()
	void HandleHeroDied(AAshHeroCharacter* Hero);

	/** Checks the base-driven win/lose conditions; ends the match if one is met. */
	void EvaluateBaseConditions();

	/** Finds the player hero and binds its death delegate (retried until the hero exists). */
	void TryBindToHero();

	/** Sets the state, broadcasting OnMatchStateChanged on a real change. */
	void SetMatchState(EAshMatchState NewState);

	/** True for the player's coalition (Player or Ally share a side). */
	static bool IsPlayerSide(EAshTeamId Team);

	/** Current lifecycle state. */
	UPROPERTY(Transient)
	EAshMatchState MatchState = EAshMatchState::NotStarted;

	/** Terminal result, set once on end. */
	UPROPERTY(Transient)
	EAshMatchResult MatchResult = EAshMatchResult::None;

	/** Reason the match ended, set once on end. */
	UPROPERTY(Transient)
	EAshMatchEndReason EndReason = EAshMatchEndReason::None;

	/** The hero we bound a death handler to (weak: never keep the avatar alive). */
	UPROPERTY(Transient)
	TWeakObjectPtr<AAshHeroCharacter> BoundHero;

	/** World time StartMatch() ran. */
	float MatchStartTime = 0.f;

	/** World time the match ended (for a frozen elapsed readout). */
	float MatchEndTime = 0.f;

	/** Retries TryBindToHero until the experience-spawned hero exists. */
	FTimerHandle HeroBindTimerHandle;

	/** True once bound to the base subsystem delegate. */
	bool bBoundToBases = false;
};
