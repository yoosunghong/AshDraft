// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "AI/AshBattleTypes.h"
#include "Engine/TimerHandle.h"
#include "AshBattleSubsystem.generated.h"

/**
 * UAshBattleSubsystem  —  the Global Combat "Engagement Director" (Phase 24, stabilised in Phase 26).
 *
 * Sits above the squad/fireteam layer in the hierarchical AI (ARCHITECTURE.md 8): it owns the
 * *global combat state*. When two hostile Generals come within encounter range it forms a Battle and
 * computes a deliberate, balanced engagement plan for every fireteam involved — which enemy fireteam
 * each fights (1v1 where possible, 1v2 for the surplus, never 1v5) and where on a ring around the
 * battle centre that duel happens. The Mass fireteam processor reads the plan and drives the soldiers
 * to march straight to their designated enemy, ignoring everything in between, then duel at the slot.
 *
 * The plan is recomputed on a slow timer (no Actor Tick — CLAUDE.md 18.3), never per frame. Phase 26
 * makes the plan *persistent*: a battle keeps its ring state (a stable orientation axis, a smoothed
 * centre, and a fixed angular slot per fireteam) across replans, so a committed fireteam keeps the
 * same slot and marches there once — instead of being handed a freshly-rotated ring every replan (the
 * old behaviour that made committed squads appear to march back and forth). Matching is delegated to
 * the shared AshEngagement helper (the same algorithm the fireteam processor's fallback uses).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshBattleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~UWorldSubsystem interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~End of UWorldSubsystem interface

	/** True while at least one Battle is live (any fireteam has a committed engagement assignment). */
	UFUNCTION(BlueprintPure, Category = "Ash|Battle")
	bool IsInCombat() const { return bGlobalCombatActive; }

	/** Number of active battles this frame (debug/telemetry). */
	UFUNCTION(BlueprintPure, Category = "Ash|Battle")
	int32 GetActiveBattleCount() const { return ActiveBattleCount; }

	/**
	 * Returns the committed engagement plan for FireteamId, if it belongs to an active battle. The Mass
	 * fireteam processor calls this every frame; the plan itself only changes on the replan cadence.
	 */
	bool GetAssignment(int32 FireteamId, FAshEngagementAssignment& OutAssignment) const;

private:
	/**
	 * Per-battle ring state persisted across replans (Phase 26). Keyed by a stable battle key (the hash
	 * of the sorted participating general ids) so the same clash keeps the same seats.
	 */
	struct FAshBattleRingState
	{
		/** Low-pass-filtered battle centre — drifts smoothly rather than snapping each replan. */
		FVector CentreEMA = FVector::ZeroVector;

		/** Orientation of angle-0 on the ring, captured once at battle formation (general-vs-general axis). */
		FVector AxisDir = FVector::ForwardVector;

		/** Fixed angular index per fireteam; survivors keep their angle even as other duels die. */
		TMap<int32, int32> SlotIndexOfFireteam;

		/** Stable divisor for the ring angles: the most slots the battle has ever held (dead seats stay gaps). */
		int32 SlotCount = 0;

		/** Last plan's fireteam -> enemy assignment, fed back as the hysteresis hint for the next replan. */
		TMap<int32, int32> PrevAssignment;

		bool bInitialised = false;
	};

	/** Timer body: detect encounters, (re)compute the battle plan for every involved fireteam. */
	void UpdateBattles();

	/** Replans now (also the timer body's worker), rebuilding Assignments from current registries. */
	void Replan();

	/** (Re)installs the replan timer at Period seconds if it differs from the current cadence. */
	void EnsureReplanTimer(float Period);

	/** Committed plan keyed by FireteamId. Rebuilt each Replan; stable between replans. */
	TMap<int32, FAshEngagementAssignment> Assignments;

	/** Persistent ring state per battle (survives replans for slot/centre/axis stability). */
	TMap<uint32, FAshBattleRingState> RingStates;

	/** Cached flags published to the fireteam processor / BP. */
	bool bGlobalCombatActive = false;
	int32 ActiveBattleCount = 0;

	/** Drives Replan at a fixed cadence (commitment matters more than instant reaction). */
	FTimerHandle ReplanTimerHandle;

	/** The replan period currently installed on the timer (adapts to the generals' authored ReplanPeriod). */
	float CurrentReplanPeriod = 0.f;

	/** Replan cadence used before any general (with a config) exists. */
	static constexpr float DefaultReplanPeriod = 0.5f;

	/** Fallbacks used when a participating non-player hero carries no UAshHeroConfig. */
	static constexpr float DefaultEncounterRadius = 3000.f;
	static constexpr float DefaultDuelRingRadius = 1400.f;
	static constexpr int32 DefaultMaxAttackersPerEnemy = 2;
	static constexpr float DefaultRingRecenterSmoothing = 0.25f;
};
