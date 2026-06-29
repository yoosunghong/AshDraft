// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassFireteamProcessor.generated.h"

/**
 * Fireteam-level AI between General StateTree and individual soldier FSM. Maintains persistent
 * five-soldier V formations, arranges fireteams around squad objectives, and assigns combat targets
 * between opposing fireteams or against hostile generals.
 */
UCLASS(config = Game)
class ASHDRAFTCORERUNTIME_API UAshMassFireteamProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassFireteamProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float FireteamSpacing = 520.f;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float FireteamRowSpacing = 460.f;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float FireteamEngageRadius = 2200.f;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float GeneralActorEngageRadius = 1800.f;

	/** Fireteams laid out per row when arranging a squad around its objective (was a hardcoded 3). */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "1"))
	int32 FireteamsPerRow = 3;

	/** Distance (cm) under which a formation-marching fireteam is considered arrived/standby (was hardcoded 150). */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float FireteamStandbyDistance = 150.f;

	/**
	 * Distance (cm) a still-marching fireteam may overshoot its enemy before engaging. Legacy Phase 23
	 * standoff offset; Phase 26 hands melee off to the per-soldier dissolve at the contact point, so this
	 * no longer offsets the engaged anchor (kept for config compatibility / the deploy approach feel).
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float CombatAnchorDistance = 420.f;

	/**
	 * Lateral spacing (cm) between adjacent soldiers of an *engaged* fireteam (Phase 27). On contact a
	 * fireteam no longer collapses every soldier onto the single contact point (which made a 1v1 squad
	 * clash ball 10 bodies onto one spot); instead its soldiers fan out abreast along the contact front,
	 * spaced this far apart, while staying clustered around the shared anchor. The opposing fireteam mirrors
	 * the line on its own side, so the two squads meet as spread ranks. The melee dissolve then hands each
	 * soldier a distinct enemy; this seats the spacing they hold between/while picking targets.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam|Engage", meta = (ClampMin = "0.0"))
	float EngageLineSpacing = 160.f;

	/**
	 * Distance (cm) an engaged fireteam's line sits back from the shared contact point on *its own side*
	 * (Phase 27). Both fireteams pull back by this much, leaving a gap they close by stepping in to strike,
	 * so opposing ranks don't spawn perfectly interpenetrating. Kept small so the squads still clearly meet.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam|Engage", meta = (ClampMin = "0.0"))
	float EngageOwnSideOffset = 120.f;

	/**
	 * Time constant (s) for low-pass filtering an engaged fireteam's facing-toward-enemy (Phase 27). The
	 * raw facing is `normalize(enemyAvg - myAvg)`, which *degenerates and jitters* once the two squads
	 * intermix and their averages coincide — collapsing the spread line back onto one point and making
	 * soldiers chase a jittering anchor. Smoothing (and never updating from a degenerate delta) keeps the
	 * opposing lines stably separated and the motion calm. 0 = no smoothing (raw, jittery).
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam|Engage", meta = (ClampMin = "0.0"))
	float EngageFacingSmoothTime = 0.25f;

	/**
	 * Distance (cm) from its assigned ring slot at which a battle-deploying fireteam hands off from
	 * Deploying (march straight to the slot, ignore everything) to Engaged (duel the designated enemy).
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam|Battle", meta = (ClampMin = "0.0"))
	float BattleSlotArrivalDistance = 350.f;

	/**
	 * Proximity (cm) to the designated enemy that also triggers the Deploying->Engaged hand-off, so two
	 * fireteams converging on the same slot start fighting on contact instead of walking through it.
	 */
	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam|Battle", meta = (ClampMin = "0.0"))
	float BattleEngageProximity = 800.f;

private:
	FMassEntityQuery EntityQuery;

	/** Last frame's out-of-battle fireteam pairing, fed back to the matcher as hysteresis (anti-flip). */
	TMap<int32, int32> LastFallbackAssignment;

	/** Per-fireteam low-pass-filtered engage facing; keeps the melee spread line stable (Phase 27). */
	TMap<int32, FVector> SmoothedEngageFacing;
};
