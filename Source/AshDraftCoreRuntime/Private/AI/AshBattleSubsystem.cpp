// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshBattleSubsystem.h"

#include "AI/AshEngagementMatcher.h"
#include "AI/AshFireteamSubsystem.h"
#include "AI/AshGeneralSubsystem.h"
#include "Character/AshGeneralCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Teams/AshTeamStatics.h"

namespace
{
	/** Draws the battle centre + duel ring slots when Ash.Battle.Debug is on. */
	static TAutoConsoleVariable<int32> CVarBattleDebug(
		TEXT("Ash.Battle.Debug"),
		0,
		TEXT("Draw the global-combat engagement plan (battle centre, duel ring slots, assignments)."),
		ECVF_Default);

	/** One participating general flattened for the planner. */
	struct FAshBattleGeneral
	{
		int32 GeneralId = INDEX_NONE;
		int32 SquadId = INDEX_NONE;
		EAshTeamId TeamId = EAshTeamId::Neutral;
		FVector Position = FVector::ZeroVector;
		const UAshHeroConfig* Config = nullptr;
	};

	/** Minimal union-find over a small general array for clustering encounters into battles. */
	struct FUnionFind
	{
		TArray<int32> Parent;
		explicit FUnionFind(int32 N) { Parent.SetNum(N); for (int32 i = 0; i < N; ++i) { Parent[i] = i; } }
		int32 Find(int32 X) { while (Parent[X] != X) { Parent[X] = Parent[Parent[X]]; X = Parent[X]; } return X; }
		void Union(int32 A, int32 B) { Parent[Find(A)] = Find(B); }
	};

	/** Stable key for a battle = hash of its sorted participating general ids. */
	uint32 MakeBattleKey(const TArray<int32>& GeneralIds)
	{
		TArray<int32> Sorted = GeneralIds;
		Sorted.Sort();
		uint32 Key = 0;
		for (int32 Id : Sorted)
		{
			Key = HashCombine(Key, GetTypeHash(Id));
		}
		return Key;
	}

	/** Lowest non-negative integer not already in Used (fills retired-duel gaps before extending the ring). */
	int32 LowestFreeSlot(const TSet<int32>& Used)
	{
		int32 Slot = 0;
		while (Used.Contains(Slot)) { ++Slot; }
		return Slot;
	}
}

void UAshBattleSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Plan on a slow repeating timer rather than per-frame (CLAUDE.md 18.3) — the whole point is that
	// each fireteam commits to its assignment between replans instead of re-deciding every frame. The
	// cadence adapts to the generals' authored ReplanPeriod once they exist (see Replan).
	EnsureReplanTimer(DefaultReplanPeriod);
}

void UAshBattleSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReplanTimerHandle);
	}
	Super::Deinitialize();
}

void UAshBattleSubsystem::EnsureReplanTimer(float Period)
{
	Period = FMath::Max(0.05f, Period);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!FMath::IsNearlyEqual(Period, CurrentReplanPeriod) || !World->GetTimerManager().IsTimerActive(ReplanTimerHandle))
	{
		World->GetTimerManager().ClearTimer(ReplanTimerHandle);
		World->GetTimerManager().SetTimer(ReplanTimerHandle, this, &UAshBattleSubsystem::UpdateBattles, Period, /*bLoop=*/true);
		CurrentReplanPeriod = Period;
	}
}

bool UAshBattleSubsystem::GetAssignment(int32 FireteamId, FAshEngagementAssignment& OutAssignment) const
{
	if (const FAshEngagementAssignment* Found = Assignments.Find(FireteamId))
	{
		OutAssignment = *Found;
		return OutAssignment.bValid;
	}
	return false;
}

void UAshBattleSubsystem::UpdateBattles()
{
	Replan();
}

void UAshBattleSubsystem::Replan()
{
	Assignments.Reset();
	bGlobalCombatActive = false;
	ActiveBattleCount = 0;

	const UWorld* World = GetWorld();
	const UAshGeneralSubsystem* GeneralSubsystem = World ? World->GetSubsystem<UAshGeneralSubsystem>() : nullptr;
	const UAshFireteamSubsystem* FireteamSubsystem = World ? World->GetSubsystem<UAshFireteamSubsystem>() : nullptr;
	if (!GeneralSubsystem || !FireteamSubsystem)
	{
		return;
	}

	// --- 1. Flatten the live generals. A general must have a body and not be dead to start a battle. ---
	TArray<FAshBattleGeneral> Generals;
	float EncounterRadius = DefaultEncounterRadius;
	float DesiredReplanPeriod = DefaultReplanPeriod;
	for (int32 GeneralId : GeneralSubsystem->GetAllGeneralIds())
	{
		const FAshGeneralState GState = GeneralSubsystem->GetGeneralState(GeneralId);
		AAshGeneralCharacter* General = GState.General.Get();
		if (!General || General->IsDead())
		{
			continue;
		}
		FAshBattleGeneral G;
		G.GeneralId = GeneralId;
		G.SquadId = GState.SquadId;
		G.TeamId = GState.TeamId;
		G.Position = General->GetActorLocation();
		G.Config = General->GetConfig();
		if (G.Config)
		{
			EncounterRadius = FMath::Max(EncounterRadius, G.Config->BattleEncounterRadius);
			DesiredReplanPeriod = FMath::Max(DesiredReplanPeriod, G.Config->ReplanPeriod);
		}
		Generals.Add(G);
	}

	// Adapt the replan cadence to the authored value now that generals (may) exist.
	EnsureReplanTimer(DesiredReplanPeriod);

	if (Generals.Num() < 2)
	{
		RingStates.Reset(); // No battles possible; drop stale ring state.
		return;
	}

	const float EncounterRadiusSq = FMath::Square(EncounterRadius);

	// --- 2. Cluster hostile generals within range into battles (connected components). ---
	FUnionFind UF(Generals.Num());
	bool bAnyEncounter = false;
	for (int32 i = 0; i < Generals.Num(); ++i)
	{
		for (int32 j = i + 1; j < Generals.Num(); ++j)
		{
			if (!UAshTeamStatics::AreTeamsHostile(Generals[i].TeamId, Generals[j].TeamId))
			{
				continue;
			}
			if (FVector::DistSquared2D(Generals[i].Position, Generals[j].Position) <= EncounterRadiusSq)
			{
				UF.Union(i, j);
				bAnyEncounter = true;
			}
		}
	}
	if (!bAnyEncounter)
	{
		RingStates.Reset();
		return;
	}

	// Group general indices by their battle (union-find root).
	TMap<int32, TArray<int32>> BattleGroups;
	for (int32 i = 0; i < Generals.Num(); ++i)
	{
		BattleGroups.FindOrAdd(UF.Find(i)).Add(i);
	}

	// Snapshot the current fireteam decomposition (published by the fireteam processor each frame).
	const TArray<FAshFireteamState> AllFireteams = FireteamSubsystem->GetAllFireteamStates();

	TSet<uint32> SeenKeys;

	// --- 3. Plan each battle. ---
	for (const TPair<int32, TArray<int32>>& BattleEntry : BattleGroups)
	{
		const TArray<int32>& GeneralIdxs = BattleEntry.Value;

		// A real battle needs at least two mutually hostile generals (a singleton root isn't a fight).
		bool bHasHostility = false;
		for (int32 a = 0; a < GeneralIdxs.Num() && !bHasHostility; ++a)
		{
			for (int32 b = a + 1; b < GeneralIdxs.Num(); ++b)
			{
				if (UAshTeamStatics::AreTeamsHostile(Generals[GeneralIdxs[a]].TeamId, Generals[GeneralIdxs[b]].TeamId))
				{
					bHasHostility = true;
					break;
				}
			}
		}
		if (!bHasHostility)
		{
			continue;
		}

		// Battle centre = mean of participating generals; ring + caps = max authored among them.
		FVector Centre = FVector::ZeroVector;
		float RingRadius = DefaultDuelRingRadius;
		int32 MaxAttackers = DefaultMaxAttackersPerEnemy;
		float Smoothing = DefaultRingRecenterSmoothing;
		TSet<int32> BattleSquads;
		TArray<int32> BattleGeneralIds;
		for (int32 GIdx : GeneralIdxs)
		{
			const FAshBattleGeneral& G = Generals[GIdx];
			Centre += G.Position;
			BattleSquads.Add(G.SquadId);
			BattleGeneralIds.Add(G.GeneralId);
			if (G.Config)
			{
				RingRadius = FMath::Max(RingRadius, G.Config->DuelRingRadius);
				MaxAttackers = FMath::Max(MaxAttackers, G.Config->MaxAttackersPerEnemyFireteam);
				Smoothing = FMath::Max(Smoothing, G.Config->RingRecenterSmoothing);
			}
		}
		Centre /= static_cast<float>(GeneralIdxs.Num());
		MaxAttackers = FMath::Max(1, MaxAttackers);
		Smoothing = FMath::Clamp(Smoothing, 0.f, 1.f);

		// Gather this battle's fireteams (alive, belonging to a participating squad).
		TArray<FAshFireteamState> BattleFireteams;
		for (const FAshFireteamState& FT : AllFireteams)
		{
			if (FT.AliveCount > 0 && BattleSquads.Contains(FT.SquadId))
			{
				BattleFireteams.Add(FT);
			}
		}
		if (BattleFireteams.Num() < 2)
		{
			continue;
		}

		// Keep each general's closest fireteams as a personal guard (Phase 27): they are excluded from
		// the duel ring so they follow the general's own combat objective and fight *around* it, instead
		// of every fireteam ringing out and leaving the generals dueling alone in an empty clearing.
		TSet<int32> GuardFireteamIds;
		for (int32 GIdx : GeneralIdxs)
		{
			const FAshBattleGeneral& G = Generals[GIdx];
			const int32 GuardCount = G.Config ? G.Config->GuardFireteamCount : 0;
			if (GuardCount <= 0)
			{
				continue;
			}
			TArray<TPair<float, int32>> ByDist; // (distSq to general, fireteam id) for this general's squad
			for (const FAshFireteamState& FT : BattleFireteams)
			{
				if (FT.SquadId == G.SquadId && FT.AliveCount > 0)
				{
					ByDist.Add({ static_cast<float>(FVector::DistSquared2D(FT.AveragePosition, G.Position)), FT.FireteamId });
				}
			}
			ByDist.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
			for (int32 i = 0; i < FMath::Min(GuardCount, ByDist.Num()); ++i)
			{
				GuardFireteamIds.Add(ByDist[i].Value);
			}
		}

		// Two-sided split (PoC): side 0 = first fireteam's team, side 1 = everything hostile to it.
		const EAshTeamId TeamA = BattleFireteams[0].TeamId;
		TArray<AshEngagement::FMatchUnit> Units;
		Units.Reserve(BattleFireteams.Num());
		bool bHasSideB = false;
		for (const FAshFireteamState& FT : BattleFireteams)
		{
			if (GuardFireteamIds.Contains(FT.FireteamId))
			{
				continue; // personal guard: stays with its general (fireteam-processor fallback drives it)
			}
			int32 Side;
			if (FT.TeamId == TeamA)
			{
				Side = 0;
			}
			else if (UAshTeamStatics::AreTeamsHostile(TeamA, FT.TeamId))
			{
				Side = 1;
				bHasSideB = true;
			}
			else
			{
				continue; // not part of this two-sided clash
			}
			Units.Add({ FT.FireteamId, FT.AveragePosition, Side });
		}
		if (!bHasSideB)
		{
			continue; // only one side has troops; nothing to pair (processor fallback engages)
		}

		// Persistent ring state for this battle (stable seats/centre/axis across replans).
		const uint32 Key = MakeBattleKey(BattleGeneralIds);
		SeenKeys.Add(Key);
		FAshBattleRingState& Ring = RingStates.FindOrAdd(Key);
		if (!Ring.bInitialised)
		{
			Ring.CentreEMA = Centre;
			// Orientation axis = direction from one side's general toward a hostile general, captured once.
			FVector Axis = FVector::ForwardVector;
			for (int32 a = 0; a < GeneralIdxs.Num(); ++a)
			{
				for (int32 b = a + 1; b < GeneralIdxs.Num(); ++b)
				{
					if (UAshTeamStatics::AreTeamsHostile(Generals[GeneralIdxs[a]].TeamId, Generals[GeneralIdxs[b]].TeamId))
					{
						FVector D = Generals[GeneralIdxs[b]].Position - Generals[GeneralIdxs[a]].Position;
						D.Z = 0.f;
						if (D.Normalize()) { Axis = D; }
						break;
					}
				}
			}
			Ring.AxisDir = Axis;
			Ring.bInitialised = true;
		}
		else
		{
			Ring.CentreEMA = FMath::Lerp(Ring.CentreEMA, Centre, Smoothing);
		}
		const float BaseAngle = FMath::Atan2(Ring.AxisDir.Y, Ring.AxisDir.X);

		// --- 3a. Balanced matching via the shared helper (same algorithm as the fireteam fallback). ---
		AshEngagement::FPairingParams Params;
		Params.MaxPairDist = 0.f; // within a battle, pair regardless of distance
		Params.MaxAttackers = MaxAttackers;
		Params.StickinessBonus = RingRadius * 0.25f; // commitment: resist re-pairing across the ring
		TArray<AshEngagement::FMatchPairing> Pairings;
		TMap<int32, int32> OutAssignment;
		AshEngagement::BalancedPairing(Units, Params, Ring.PrevAssignment, Pairings, OutAssignment);
		if (Pairings.Num() == 0)
		{
			continue;
		}

		// --- 3b. Assign each duel a *persistent* ring slot. A fireteam keeps its angular index for the
		//         battle's life; a retired duel leaves a gap (filled by the next new duel) so survivors
		//         never shift angle. The divisor (SlotCount) only ever grows, so angles stay put. ---
		TSet<int32> UsedSlots;
		TArray<int32> PairingSlot;
		PairingSlot.SetNum(Pairings.Num());
		TMap<int32, int32> FreshSlotIndex; // rebuilt this replan (prunes dead fireteams)
		for (int32 p = 0; p < Pairings.Num(); ++p)
		{
			const int32 FocalA = Pairings[p].A;
			const int32 FocalB = Pairings[p].B;
			int32 Slot = INDEX_NONE;
			if (const int32* Existing = Ring.SlotIndexOfFireteam.Find(FocalA))
			{
				Slot = *Existing;
			}
			else if (const int32* ExistingB = Ring.SlotIndexOfFireteam.Find(FocalB))
			{
				Slot = *ExistingB;
			}
			if (Slot == INDEX_NONE || UsedSlots.Contains(Slot))
			{
				Slot = LowestFreeSlot(UsedSlots);
			}
			UsedSlots.Add(Slot);
			PairingSlot[p] = Slot;
			FreshSlotIndex.Add(FocalA, Slot);
			FreshSlotIndex.Add(FocalB, Slot);
		}
		int32 MaxUsed = 0;
		for (int32 S : UsedSlots) { MaxUsed = FMath::Max(MaxUsed, S + 1); }
		Ring.SlotCount = FMath::Max(Ring.SlotCount, MaxUsed);
		const int32 SlotDivisor = FMath::Max(1, Ring.SlotCount);
		Ring.SlotIndexOfFireteam = MoveTemp(FreshSlotIndex);

		// --- 3c. Emit assignments: both focals fight each other at the slot; extras share it. ---
		TArray<FVector> DuelSlots;
		DuelSlots.SetNum(Pairings.Num());
		for (int32 p = 0; p < Pairings.Num(); ++p)
		{
			const float Angle = BaseAngle + (2.f * PI * PairingSlot[p]) / static_cast<float>(SlotDivisor);
			FVector Slot = Ring.CentreEMA + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * RingRadius;
			Slot.Z = Ring.CentreEMA.Z;
			DuelSlots[p] = Slot;

			const AshEngagement::FMatchPairing& P = Pairings[p];
			FAshEngagementAssignment& AA = Assignments.FindOrAdd(P.A);
			AA.bValid = true; AA.EnemyFireteamId = P.B; AA.RingSlot = Slot;
			FAshEngagementAssignment& AB = Assignments.FindOrAdd(P.B);
			AB.bValid = true; AB.EnemyFireteamId = P.A; AB.RingSlot = Slot;
			for (int32 ExtraId : P.Extras)
			{
				FAshEngagementAssignment& AE = Assignments.FindOrAdd(ExtraId);
				AE.bValid = true;
				AE.RingSlot = Slot;
				if (const int32* Enemy = OutAssignment.Find(ExtraId)) { AE.EnemyFireteamId = *Enemy; }
			}
		}

		Ring.PrevAssignment = MoveTemp(OutAssignment);
		++ActiveBattleCount;

		if (CVarBattleDebug.GetValueOnGameThread() != 0 && World)
		{
			const float Life = CurrentReplanPeriod + 0.05f;
			DrawDebugSphere(const_cast<UWorld*>(World), Ring.CentreEMA, 120.f, 12, FColor::Yellow, false, Life);
			DrawDebugCircle(const_cast<UWorld*>(World), Ring.CentreEMA, RingRadius, 48, FColor::Orange, false,
				Life, 0, 6.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
			for (const FVector& Slot : DuelSlots)
			{
				DrawDebugSphere(const_cast<UWorld*>(World), Slot, 90.f, 8, FColor::Red, false, Life);
				DrawDebugLine(const_cast<UWorld*>(World), Ring.CentreEMA, Slot, FColor::Silver, false, Life, 0, 3.f);
			}
		}
	}

	// Drop ring state for battles that no longer exist (prevents stale-key leak).
	for (auto It = RingStates.CreateIterator(); It; ++It)
	{
		if (!SeenKeys.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	bGlobalCombatActive = Assignments.Num() > 0;
}
