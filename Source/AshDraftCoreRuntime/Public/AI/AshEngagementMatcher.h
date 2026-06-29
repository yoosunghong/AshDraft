// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * AshEngagementMatcher.h  —  the shared engagement-matching algorithms (Phase 26).
 *
 * One dependency-free home (no Mass, no UObject, no World types) for the combat pairing math that was
 * previously duplicated between UAshBattleSubsystem and UAshMassFireteamProcessor. Two cooperating
 * primitives, both used by more than one caller:
 *
 *  - BalancedPairing      — greedy 1:1 by mutual proximity + capped surplus doubling + hysteresis.
 *                           Pairs two sides of "units" (fireteams). Used by the Battle director to lay
 *                           out duels on the ring, and by the fireteam processor's out-of-battle fallback.
 *  - AcquireCappedNearest — pick the nearest hostile whose attacker count is under a cap, preferring to
 *                           keep the current target (stickiness). The melee-dissolve primitive: run per
 *                           soldier it spreads attackers across distinct enemies and lets the surplus
 *                           reach deeper (open) enemies — the source of the jumbled front + rear engagement.
 *
 * Pure and side-effect-light (AcquireCappedNearest mutates only the caller's claim map): unit-testable
 * without spinning up a world. CLAUDE.md: data-oriented, no god objects — this kills the duplication.
 */
namespace AshEngagement
{
	/** One unit to be matched. Side splits the two opposing groups; only Side 0 pairs with Side 1. */
	struct FMatchUnit
	{
		int32 Id = INDEX_NONE;
		FVector Pos = FVector::ZeroVector;
		int32 Side = 0;
	};

	/** Tunables for BalancedPairing. All data-driven from the configs at the call site. */
	struct FPairingParams
	{
		/** Max planar distance two units may be paired across. 0 = unlimited. */
		float MaxPairDist = 0.f;

		/** Cap on how many units may pile onto one enemy focal (1v1 -> 1v2, never 1v5). */
		int32 MaxAttackers = 2;

		/** Effective-distance discount (cm) given to a pair that existed last plan — commitment/hysteresis. */
		float StickinessBonus = 0.f;
	};

	/** One duel: two focals (A on side 0, B on side 1) plus any surplus units doubled onto either focal. */
	struct FMatchPairing
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		TArray<int32> Extras;
	};

	/**
	 * Greedy balanced matching of the Units' two sides.
	 *  - OutPairings: one entry per 1:1 duel (with Extras = surplus units sharing that duel).
	 *  - OutAssignment: every matched unit Id -> the enemy focal Id it is ordered to fight.
	 * PrevAssignment (last plan's Id->Id) supplies the hysteresis bonus so committed pairs stick.
	 */
	ASHDRAFTCORERUNTIME_API void BalancedPairing(
		TArrayView<const FMatchUnit> Units,
		const FPairingParams& Params,
		const TMap<int32, int32>& PrevAssignment,
		TArray<FMatchPairing>& OutPairings,
		TMap<int32, int32>& OutAssignment);

	/**
	 * Pick the best melee target for one soldier from its local hostile Candidates.
	 *  - Nearest candidate whose ClaimCounts < MaxAttackers, EXCEPT the current target (PreferKeep) may be
	 *    kept even at cap (it already holds its slot) and gets a KeepStickiness distance discount.
	 *  - ClaimCounts is updated for the delta vs PreferKeep (release old / claim new) so a batch of calls
	 *    sharing one map distributes attackers across distinct enemies without thrashing.
	 * Returns the chosen Id (INDEX_NONE if none in range) and its position in OutPos.
	 */
	ASHDRAFTCORERUNTIME_API int32 AcquireCappedNearest(
		const FVector& From,
		float MaxRange,
		int32 MaxAttackers,
		int32 PreferKeep,
		float KeepStickiness,
		TArrayView<const FMatchUnit> Candidates,
		TMap<int32, int32>& ClaimCounts,
		FVector& OutPos);
}
