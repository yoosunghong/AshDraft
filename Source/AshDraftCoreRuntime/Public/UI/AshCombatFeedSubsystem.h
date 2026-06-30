// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "UI/AshUITypes.h"
#include "AshCombatFeedSubsystem.generated.h"

class UTexture2D;

/** Player kill count changed (carries the new total). The HUD binds here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAshOnPlayerKillCountChanged, int32, NewKillCount);

/** The player struck a unit (carries the struck-unit snapshot). The HUD binds here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAshOnUnitStruck, const FAshStruckUnitInfo&, Info);

/**
 * UAshCombatFeedSubsystem
 *
 * The UI-facing combat feed (Phase 30, ARCHITECTURE.md 16). A world-scoped hub that the HUD reads to
 * show the player's kill count and the most-recently-struck enemy (name / affiliation / health). It
 * only *observes* combat — it owns no gameplay logic and holds no hard references to units; the damage
 * choke points push strikes in via the static Get() accessor (the soldier proxy's actor-space melee
 * hit and the GAS attribute-set damage path), and the widgets read the state back through the
 * BlueprintAssignable delegates / pure getters below. This keeps the dependency one-way (gameplay ->
 * feed -> UI), never UI -> gameplay (ARCHITECTURE.md 18.4).
 *
 * Only strikes whose instigator is the local player pawn are recorded, so the HUD reflects the
 * player's own combat (AI-vs-AI damage is ignored here — telemetry logs that separately, Phase 17).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshCombatFeedSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor for non-subsystem callers (the damage paths). May be null. */
	static UAshCombatFeedSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Records a strike landed by Instigator on a unit. No-op unless Instigator is the local player
	 * pawn. Updates the last-struck snapshot (and broadcasts OnUnitStruck); if bKilled, also bumps the
	 * kill count (and broadcasts OnPlayerKillCountChanged). UnitName/Team/HealthFraction describe the
	 * victim for the HUD's recently-struck panel.
	 */
	void ReportPlayerStrike(const AActor* Instigator, const FText& UnitName, EAshTeamId Team, float HealthFraction, bool bKilled, UTexture2D* Portrait = nullptr);

	/** Total units the player has killed this match. */
	UFUNCTION(BlueprintPure, Category = "Ash|UI")
	int32 GetPlayerKillCount() const { return PlayerKillCount; }

	/** The most recently struck unit (bValid is false until the first strike). */
	UFUNCTION(BlueprintPure, Category = "Ash|UI")
	const FAshStruckUnitInfo& GetLastStruckUnit() const { return LastStruckUnit; }

	/** Resets the feed (e.g. on a new match). */
	UFUNCTION(BlueprintCallable, Category = "Ash|UI")
	void ResetFeed();

	/** Fired whenever the player's kill count changes. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|UI")
	FAshOnPlayerKillCountChanged OnPlayerKillCountChanged;

	/** Fired whenever the player strikes a unit. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|UI")
	FAshOnUnitStruck OnUnitStruck;

private:
	/** True if Actor is the local player's controlled pawn. */
	bool IsLocalPlayerPawn(const AActor* Actor) const;

	/** Running player kill count. */
	UPROPERTY(Transient)
	int32 PlayerKillCount = 0;

	/** Snapshot of the last unit the player struck. */
	UPROPERTY(Transient)
	FAshStruckUnitInfo LastStruckUnit;
};
