// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshEngagementMatcher.h"

void AshEngagement::BalancedPairing(
	TArrayView<const FMatchUnit> Units,
	const FPairingParams& Params,
	const TMap<int32, int32>& PrevAssignment,
	TArray<FMatchPairing>& OutPairings,
	TMap<int32, int32>& OutAssignment)
{
	OutPairings.Reset();
	OutAssignment.Reset();

	// Split the field into the two opposing sides and remember each unit's position by Id (for leftovers).
	TArray<int32> SideA;
	TArray<int32> SideB;
	TMap<int32, FVector> PosOfId;
	PosOfId.Reserve(Units.Num());
	for (int32 i = 0; i < Units.Num(); ++i)
	{
		PosOfId.Add(Units[i].Id, Units[i].Pos);
		(Units[i].Side == 0 ? SideA : SideB).Add(i);
	}
	if (SideA.Num() == 0 || SideB.Num() == 0)
	{
		return; // Only one side present: nothing to pair (caller's fallback handles it).
	}

	const float MaxDistSq = (Params.MaxPairDist > 0.f) ? FMath::Square(Params.MaxPairDist) : TNumericLimits<float>::Max();
	const float Sticky = FMath::Max(0.f, Params.StickinessBonus);
	const int32 Cap = FMath::Max(1, Params.MaxAttackers);

	// All cross-side candidate pairs, scored by effective (hysteresis-adjusted) distance.
	struct FCand { int32 IA; int32 IB; float Eff; };
	TArray<FCand> Cands;
	Cands.Reserve(SideA.Num() * SideB.Num());
	for (int32 IA : SideA)
	{
		for (int32 IB : SideB)
		{
			const float D2 = FVector::DistSquared2D(Units[IA].Pos, Units[IB].Pos);
			if (D2 > MaxDistSq)
			{
				continue;
			}
			float Eff = FMath::Sqrt(D2);
			const int32* PrevA = PrevAssignment.Find(Units[IA].Id);
			const int32* PrevB = PrevAssignment.Find(Units[IB].Id);
			const bool bWasPaired = (PrevA && *PrevA == Units[IB].Id) || (PrevB && *PrevB == Units[IA].Id);
			if (bWasPaired)
			{
				Eff -= Sticky; // committed pair resists being broken by a marginally-closer rival
			}
			Cands.Add({ IA, IB, Eff });
		}
	}
	Cands.Sort([](const FCand& L, const FCand& R) { return L.Eff < R.Eff; });

	// Greedy 1:1: take the closest still-free cross-side pair, bind it as a duel.
	TSet<int32> Used;
	TMap<int32, int32> AttackersOn;     // focal Id -> attacker count
	TMap<int32, int32> PairingOfFocal;  // focal Id -> OutPairings index
	for (const FCand& C : Cands)
	{
		if (Used.Contains(C.IA) || Used.Contains(C.IB))
		{
			continue;
		}
		Used.Add(C.IA);
		Used.Add(C.IB);
		FMatchPairing P;
		P.A = Units[C.IA].Id;
		P.B = Units[C.IB].Id;
		const int32 Idx = OutPairings.Add(P);
		OutAssignment.Add(P.A, P.B);
		OutAssignment.Add(P.B, P.A);
		AttackersOn.Add(P.A, 1);
		AttackersOn.Add(P.B, 1);
		PairingOfFocal.Add(P.A, Idx);
		PairingOfFocal.Add(P.B, Idx);
	}

	if (OutPairings.Num() == 0)
	{
		return; // No 1:1 possible (e.g. all out of range): leave unassigned for the caller's fallback.
	}

	// Surplus side doubles up: each unused unit attaches to the nearest opposite-side focal that is still
	// under the attacker cap (1v1 -> 1v2), only overflowing past the cap when every focal is full.
	auto AttachLeftovers = [&](const TArray<int32>& Side, bool bSideIsA)
	{
		for (int32 UI : Side)
		{
			if (Used.Contains(UI))
			{
				continue;
			}
			const FVector MyPos = Units[UI].Pos;
			int32 BestFocal = INDEX_NONE;
			int32 BestFocalCapped = INDEX_NONE;
			float BestDistSq = TNumericLimits<float>::Max();
			float BestDistSqCapped = TNumericLimits<float>::Max();
			for (const FMatchPairing& P : OutPairings)
			{
				const int32 Focal = bSideIsA ? P.B : P.A; // a side-A leftover fights a side-B focal
				const float DistSq = FVector::DistSquared2D(MyPos, PosOfId.FindRef(Focal));
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestFocal = Focal;
				}
				if (AttackersOn.FindRef(Focal) < Cap && DistSq < BestDistSqCapped)
				{
					BestDistSqCapped = DistSq;
					BestFocalCapped = Focal;
				}
			}
			const int32 ChosenFocal = (BestFocalCapped != INDEX_NONE) ? BestFocalCapped : BestFocal;
			if (ChosenFocal == INDEX_NONE)
			{
				continue;
			}
			AttackersOn.FindOrAdd(ChosenFocal)++;
			OutPairings[PairingOfFocal[ChosenFocal]].Extras.Add(Units[UI].Id);
			OutAssignment.Add(Units[UI].Id, ChosenFocal);
			Used.Add(UI);
		}
	};
	AttachLeftovers(SideA, /*bSideIsA=*/true);
	AttachLeftovers(SideB, /*bSideIsA=*/false);
}

int32 AshEngagement::AcquireCappedNearest(
	const FVector& From,
	float MaxRange,
	int32 MaxAttackers,
	int32 PreferKeep,
	float KeepStickiness,
	TArrayView<const FMatchUnit> Candidates,
	TMap<int32, int32>& ClaimCounts,
	FVector& OutPos)
{
	const float MaxRangeSq = (MaxRange > 0.f) ? FMath::Square(MaxRange) : TNumericLimits<float>::Max();
	const int32 Cap = FMath::Max(1, MaxAttackers);

	int32 Best = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();
	FVector BestPos = FVector::ZeroVector;

	for (const FMatchUnit& C : Candidates)
	{
		const float DistSq = FVector::DistSquared2D(From, C.Pos);
		if (DistSq > MaxRangeSq)
		{
			continue;
		}
		const bool bKeep = (C.Id == PreferKeep && PreferKeep != INDEX_NONE);
		// A fresh target must be under cap; the current target may be kept even at cap (it already holds
		// its claim from the pass-0 tally), which is what stops attackers thrashing between enemies.
		if (!bKeep && ClaimCounts.FindRef(C.Id) >= Cap)
		{
			continue;
		}
		float Score = FMath::Sqrt(DistSq);
		if (bKeep)
		{
			Score = FMath::Max(0.f, Score - KeepStickiness);
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = C.Id;
			BestPos = C.Pos;
		}
	}

	// Update the shared claim map for the delta vs the previous target only (keeping = no change, so the
	// pass-0 baseline stays correct regardless of evaluation order across the batch).
	if (Best != PreferKeep)
	{
		if (PreferKeep != INDEX_NONE)
		{
			if (int32* Old = ClaimCounts.Find(PreferKeep))
			{
				*Old = FMath::Max(0, *Old - 1);
			}
		}
		if (Best != INDEX_NONE)
		{
			ClaimCounts.FindOrAdd(Best)++;
		}
	}

	OutPos = BestPos;
	return Best;
}
