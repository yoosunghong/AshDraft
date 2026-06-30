// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassSoldierBehaviorProcessor.h"

#include "AI/AshEngagementMatcher.h"
#include "Engine/World.h"
#include "Mass/AshSoldierBehaviorConfig.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"
#include "Teams/AshTeamStatics.h"

namespace
{
	/** Fallbacks for soldiers with no behavior config (mirror UAshSoldierBehaviorConfig defaults). */
	constexpr int32 DefaultMaxAttackers = 4;
	constexpr int32 DefaultActiveAttackerCount = 3;
	constexpr float DefaultMeleeChaseRadius = 700.f;
	constexpr float DefaultMeleeStickiness = 150.f;

	/**
	 * Minimal uniform 2D spatial hash for *local* hostile sensing. Cells are sized to the largest
	 * sense radius in play so a 3x3 cell neighbourhood always covers any soldier's search. File-local:
	 * it is an implementation detail of the behavior pass (mirrors the combat processor's grid).
	 */
	struct FAshSenseGrid
	{
		struct FEntry
		{
			FMassEntityHandle Handle;
			FVector Position = FVector::ZeroVector;
			EAshTeamId Team = EAshTeamId::Neutral;
			bool bAlive = false;
		};

		explicit FAshSenseGrid(float InCellSize)
			: CellSize(FMath::Max(1.f, InCellSize))
		{
		}

		FIntPoint CellOf(const FVector& P) const
		{
			return FIntPoint(FMath::FloorToInt(P.X / CellSize), FMath::FloorToInt(P.Y / CellSize));
		}

		void Add(const FEntry& Entry)
		{
			const int32 EntryIndex = Entries.Add(Entry);
			Cells.FindOrAdd(CellOf(Entry.Position)).Add(EntryIndex);
		}

		/**
		 * Collect every living entry hostile to SeekerTeam within MaxRange of From (excluding Self) as
		 * match units (Id = entity index) for the engagement matcher. Out is reset, not freed, so the
		 * caller can reuse one scratch array across soldiers (allocation-free after warmup).
		 */
		void GatherHostiles(const FMassEntityHandle& Self, const FVector& From, EAshTeamId SeekerTeam,
			float MaxRange, TArray<AshEngagement::FMatchUnit>& Out) const
		{
			Out.Reset();
			const FIntPoint Base = CellOf(From);
			const float MaxRangeSq = MaxRange * MaxRange;
			for (int32 dx = -1; dx <= 1; ++dx)
			{
				for (int32 dy = -1; dy <= 1; ++dy)
				{
					const TArray<int32>* Bucket = Cells.Find(FIntPoint(Base.X + dx, Base.Y + dy));
					if (!Bucket)
					{
						continue;
					}
					for (int32 EntryIndex : *Bucket)
					{
						const FEntry& E = Entries[EntryIndex];
						if (!E.bAlive || E.Handle == Self || !UAshTeamStatics::AreTeamsHostile(SeekerTeam, E.Team))
						{
							continue;
						}
						if (FVector::DistSquared2D(From, E.Position) <= MaxRangeSq)
						{
							Out.Add({ E.Handle.Index, E.Position, 0 });
						}
					}
				}
			}
		}

		float CellSize;
		TArray<FEntry> Entries;
		TMap<FIntPoint, TArray<int32>> Cells;
	};
}

UAshMassSoldierBehaviorProcessor::UAshMassSoldierBehaviorProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// The state machine must decide targets/state before movement steers and before combat strikes.
	ExecutionOrder.ExecuteBefore.Add(TEXT("AshMassMovementProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("AshMassCombatProcessor"));
	// Team statics + squad subsystem read on the game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassSoldierBehaviorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatTargetFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshSoldierStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshBehaviorFragment>(EMassFragmentAccess::ReadOnly);
	// Read-only so the Deploying global-combat state can suppress local sensing and the Engaged state
	// can supply the melee contact anchor (Phase 24/26).
	EntityQuery.AddRequirement<FAshFormationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshLODFragment>(EMassFragmentAccess::ReadWrite);
}

void UAshMassSoldierBehaviorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = Context.GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

	const auto EffectiveSense = [this](const UAshSoldierBehaviorConfig* Cfg)
	{
		return Cfg ? Cfg->LocalSenseRadius : DefaultLocalSenseRadius;
	};

	// Resets the engagement so the soldier rejoins its squad's advance.
	const auto ClearEngage = [](FAshSoldierStateFragment& S)
	{
		S.State = EAshSoldierState::FollowSquad;
		S.bEngaged = false;
	};

	// --- Pass 0: size the grid to the largest sense radius so a 3x3 neighbourhood always covers it. ---
	float MaxSense = DefaultLocalSenseRadius;
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshBehaviorFragment> BehaviorList = ChunkContext.GetFragmentView<FAshBehaviorFragment>();
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			MaxSense = FMath::Max(MaxSense, EffectiveSense(BehaviorList[It].Behavior));
		}
	});

	if (MaxSense <= 0.f)
	{
		return; // No soldier senses anything; nothing to do.
	}

	// --- Pass A: snapshot every soldier into the sense grid + tally current melee claims (how many of
	// each enemy is already targeted) so the dissolve can distribute attackers across distinct foes. We
	// also tally how many soldiers currently hold an INNER (striking) slot on each target — both Mass
	// enemies and actor targets (general / hero) — so the surround pass keeps only ActiveAttackerCount
	// strikers per target and rings the rest (Phase 28). A soldier is an inner holder when its current
	// state is Engage/Attack; a Surround soldier still counts as a committer (ClaimCounts) but not an
	// inner striker, so it occupies a ring slot under the cap without taking a striking slot. ---
	FAshSenseGrid Grid(MaxSense);
	TMap<int32, FMassEntityHandle> HandleOfIndex; // entity index -> handle (resolve the matcher's choice)
	TMap<int32, int32> ClaimCounts;               // enemy entity index -> total committers (inner + surround)
	TMap<int32, int32> InnerUsed;                 // enemy entity index -> inner (striking) holders
	TMap<uint32, int32> ActorInnerUsed;           // actor target unique id -> inner (striking) holders
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshCombatFragment> CombatList = ChunkContext.GetFragmentView<FAshCombatFragment>();
		const TConstArrayView<FAshCombatTargetFragment> CombatTargetList = ChunkContext.GetFragmentView<FAshCombatTargetFragment>();
		const TConstArrayView<FAshSoldierStateFragment> StateList = ChunkContext.GetFragmentView<FAshSoldierStateFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const FMassEntityHandle Handle = ChunkContext.GetEntity(It);
			const bool bAlive = HealthList[It].CurrentHealth > 0.f;

			FAshSenseGrid::FEntry Entry;
			Entry.Handle = Handle;
			Entry.Position = MovementList[It].Position;
			Entry.Team = TeamList[It].TeamId;
			Entry.bAlive = bAlive;
			Grid.Add(Entry);
			HandleOfIndex.Add(Handle.Index, Handle);

			if (!bAlive)
			{
				continue;
			}

			const EAshSoldierState St = StateList[It].State;
			// A soldier on its post-cycle cooldown (Phase 29) is not an "active striker", so it no longer
			// occupies an inner attack slot — freeing it for a waiting (Surround) soldier to take a turn.
			const FAshCombatFragment& CombatA = CombatList[It];
			const bool bActiveStrikerA = (CombatA.ComboLength > 0) || (CombatA.TimeSinceLastAttack >= CombatA.RolledAttackInterval);
			const bool bInnerHolder = (St == EAshSoldierState::Engage || St == EAshSoldierState::Attack) && bActiveStrikerA;

			// A living soldier's current target is a committed claim (kept unless it re-decides this frame).
			if (CombatList[It].Target.IsSet())
			{
				const int32 Ti = CombatList[It].Target.Index;
				ClaimCounts.FindOrAdd(Ti)++;
				if (bInnerHolder)
				{
					InnerUsed.FindOrAdd(Ti)++;
				}
			}
			else if (bInnerHolder && CombatTargetList[It].TargetType == EAshCombatTargetType::Actor && CombatTargetList[It].ActorTarget)
			{
				ActorInnerUsed.FindOrAdd(CombatTargetList[It].ActorTarget->GetUniqueID())++;
			}
		}
	});

	// --- Pass B: per soldier, run the local state machine / melee dissolve (throttled by AI LOD). ---
	TArray<AshEngagement::FMatchUnit> Candidates; // reused scratch (allocation-free after warmup)
	const TArrayView<const AshEngagement::FMatchUnit> EmptyCandidates;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshBehaviorFragment> BehaviorList = ChunkContext.GetFragmentView<FAshBehaviorFragment>();
		const TConstArrayView<FAshFormationFragment> FormationList = ChunkContext.GetFragmentView<FAshFormationFragment>();
		const TArrayView<FAshCombatFragment> CombatList = ChunkContext.GetMutableFragmentView<FAshCombatFragment>();
		const TArrayView<FAshCombatTargetFragment> CombatTargetList = ChunkContext.GetMutableFragmentView<FAshCombatTargetFragment>();
		const TArrayView<FAshSoldierStateFragment> StateList = ChunkContext.GetMutableFragmentView<FAshSoldierStateFragment>();
		const TArrayView<FAshLODFragment> LODList = ChunkContext.GetMutableFragmentView<FAshLODFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			FAshCombatFragment& Combat = CombatList[It];
			FAshCombatTargetFragment& CombatTarget = CombatTargetList[It];
			FAshSoldierStateFragment& StateFrag = StateList[It];

			// Last frame's state + target — used to keep an inner (striking) slot stable once held, so a
			// soldier doesn't flip between striking and surrounding every AI tick (Phase 28).
			const EAshSoldierState PrevState = StateFrag.State;

			// On its post-cycle cooldown (Phase 29) a soldier is not an active striker: it yields its inner
			// attack slot so a waiting (Surround) soldier can strike, keeping its target and ringing/menacing
			// until the 3 s cooldown elapses. Mirrors the combat processor's cycle gate.
			const bool bActiveStriker = (Combat.ComboLength > 0) || (Combat.TimeSinceLastAttack >= Combat.RolledAttackInterval);

			// Releases this soldier's current melee claim (used whenever it stops targeting a Mass enemy).
			const int32 CurrentIdx = Combat.Target.IsSet() ? Combat.Target.Index : INDEX_NONE;
			const auto ReleaseClaim = [&]()
			{
				if (CurrentIdx != INDEX_NONE)
				{
					if (int32* C = ClaimCounts.Find(CurrentIdx)) { *C = FMath::Max(0, *C - 1); }
				}
			};

			// Dead soldiers hold no target/state; the death processor cleans them up.
			if (HealthList[It].CurrentHealth <= 0.f)
			{
				ReleaseClaim();
				Combat.Target.Reset();
				ClearEngage(StateFrag);
				continue;
			}

			FAshLODFragment& LOD = LODList[It];
			const float Elapsed = WorldTime - LOD.LastUpdateTime;
			// AI LOD time slicing: only re-decide once per UpdateInterval (this processor owns the slice).
			if (Elapsed < LOD.UpdateInterval)
			{
				continue;
			}
			LOD.LastUpdateTime = WorldTime;

			const FMassEntityHandle Self = ChunkContext.GetEntity(It);
			const FVector SelfPos = MovementList[It].Position;
			const EAshTeamId SelfTeam = TeamList[It].TeamId;
			const UAshSoldierBehaviorConfig* Cfg = BehaviorList[It].Behavior;
			const FAshFormationFragment& Formation = FormationList[It];

			// Global Combat deploy (Phase 24): the fireteam is marching to its ring slot; suppress local
			// sensing entirely so it ignores every enemy it passes (hands off to melee once it arrives).
			if (Formation.FireteamState == EAshFireteamState::Deploying)
			{
				ReleaseClaim();
				Combat.Target.Reset();
				ClearEngage(StateFrag);
				continue;
			}

			const int32 Active = Cfg ? FMath::Max(1, Cfg->ActiveAttackerCount) : DefaultActiveAttackerCount;

			// Actor target (rival general / hero) set by the fireteam processor: face + close, no Mass target.
			// Surround applies here too: only the first Active soldiers strike the actor; the rest ring it and
			// menace (the crowd around the hero / a lone general — the iconic Musou shot).
			if (CombatTarget.TargetType == EAshCombatTargetType::Actor && CombatTarget.ActorTarget)
			{
				ReleaseClaim();
				Combat.Target.Reset();
				if (!StateFrag.bEngaged) { StateFrag.EngageAnchor = SelfPos; StateFrag.bEngaged = true; }

				const FVector ActorPos = CombatTarget.ActorTarget->GetActorLocation();

				// Keep an inner (striking) seat if we already held one; else take a free one; else surround.
				// A soldier on its post-cycle cooldown is never inner — it yields the slot and rings (Phase 29).
				const bool bWasInner = bActiveStriker && (PrevState == EAshSoldierState::Engage || PrevState == EAshSoldierState::Attack);
				bool bInner = bWasInner;
				if (!bInner && bActiveStriker)
				{
					int32& Inner = ActorInnerUsed.FindOrAdd(CombatTarget.ActorTarget->GetUniqueID());
					if (Inner < Active) { ++Inner; bInner = true; }
				}

				if (bInner)
				{
					const float DistSq = FVector::DistSquared2D(SelfPos, ActorPos);
					StateFrag.State = (DistSq <= FMath::Square(Combat.AttackRange))
						? EAshSoldierState::Attack : EAshSoldierState::Engage;
				}
				else
				{
					// Entering the surround ring: seed the orbit bearing to our own approach side (the
					// movement processor drifts it from here). Keep it if we were already surrounding.
					if (PrevState != EAshSoldierState::Surround)
					{
						StateFrag.SlotAngle = FMath::Atan2(SelfPos.Y - ActorPos.Y, SelfPos.X - ActorPos.X);
					}
					StateFrag.State = EAshSoldierState::Surround;
				}
				continue;
			}

			const float Sense = EffectiveSense(Cfg);
			const int32 MaxAttackers = Cfg ? Cfg->MaxAttackersPerEnemySoldier : DefaultMaxAttackers;
			const float Stickiness = Cfg ? Cfg->MeleeTargetStickiness : DefaultMeleeStickiness;

			// Leash: keep the brawl local. While Engaged in a battle the leash is anchored to the fireteam's
			// contact point (the ring slot) with MeleeChaseRadius — soldiers may cross the original line to
			// reach an open enemy but the squad does not disperse. Otherwise it is the engage-anchor leash
			// (Phase 20.1): fight what crosses your path, but don't chase a straggler across the map.
			bool bLeashOK = true;
			if (Formation.FireteamState == EAshFireteamState::Engaged && Formation.bHasContactAnchor)
			{
				const float MeleeRadius = Cfg ? Cfg->MeleeChaseRadius : DefaultMeleeChaseRadius;
				bLeashOK = (MeleeRadius <= 0.f)
					|| (FVector::DistSquared2D(Formation.ContactAnchor, SelfPos) <= FMath::Square(MeleeRadius));
				// Anchor the engage state to the contact so a later fall-through to the stray leash is sane.
				StateFrag.EngageAnchor = Formation.ContactAnchor;
				StateFrag.bEngaged = true;
			}
			else
			{
				const float MaxChase = Cfg ? Cfg->MaxLeashFromObjective : 1200.f;
				const FVector Anchor = StateFrag.bEngaged ? StateFrag.EngageAnchor : SelfPos;
				bLeashOK = (MaxChase <= 0.f)
					|| (FVector::DistSquared2D(Anchor, SelfPos) <= FMath::Square(MaxChase));
			}

			// Gather local hostiles (only when within leash; otherwise no candidates -> drop + return).
			if (bLeashOK)
			{
				Grid.GatherHostiles(Self, SelfPos, SelfTeam, Sense, Candidates);
			}

			// Distinct, capped acquisition: the nearest enemy still under its attacker cap (the current
			// target stays unless it dies/leaves), so attackers spread across distinct foes and the surplus
			// reaches deeper, open enemies — the jumbled front + organic rear engagement (Phase 26).
			FVector TargetPos = FVector::ZeroVector;
			const int32 ChosenIdx = AshEngagement::AcquireCappedNearest(
				SelfPos, Sense, MaxAttackers, CurrentIdx, Stickiness,
				bLeashOK ? TArrayView<const AshEngagement::FMatchUnit>(Candidates) : EmptyCandidates,
				ClaimCounts, TargetPos);

			if (ChosenIdx == INDEX_NONE)
			{
				// No reachable enemy (none sensed, or strayed past the leash): rejoin the squad advance.
				Combat.Target.Reset();
				ClearEngage(StateFrag);
				continue;
			}

			// First contact in a stray skirmish: anchor the chase leash here (battle anchored above).
			if (!StateFrag.bEngaged)
			{
				StateFrag.bEngaged = true;
				StateFrag.EngageAnchor = SelfPos;
			}

			Combat.Target = HandleOfIndex.FindRef(ChosenIdx);

			// Inner (striking) vs outer (Surround) ring. Keep an inner seat if we already held one ON THIS
			// target; else take a free striking slot if the target still has fewer than Active strikers; else
			// ring it and menace. The surplus that can't even get a ring slot was already dropped by the cap
			// in AcquireCappedNearest (it went to a deeper, open enemy — rear engagement). (Phase 28)
			// A soldier on its post-cycle cooldown is never inner — it yields the striking slot and rings
			// the same target until its cooldown elapses, so a waiting soldier strikes in its place (Phase 29).
			const bool bWasInnerSame = bActiveStriker && (CurrentIdx == ChosenIdx)
				&& (PrevState == EAshSoldierState::Engage || PrevState == EAshSoldierState::Attack);
			const bool bWasOuterSame = (CurrentIdx == ChosenIdx) && (PrevState == EAshSoldierState::Surround);
			bool bInner = bWasInnerSame;
			if (!bInner && bActiveStriker)
			{
				int32& Inner = InnerUsed.FindOrAdd(ChosenIdx);
				if (Inner < Active) { ++Inner; bInner = true; }
			}

			if (bInner)
			{
				const float DistSq = FVector::DistSquared2D(SelfPos, TargetPos);
				StateFrag.State = (DistSq <= FMath::Square(Combat.AttackRange))
					? EAshSoldierState::Attack
					: EAshSoldierState::Engage;
			}
			else
			{
				if (!bWasOuterSame)
				{
					StateFrag.SlotAngle = FMath::Atan2(SelfPos.Y - TargetPos.Y, SelfPos.X - TargetPos.X);
				}
				StateFrag.State = EAshSoldierState::Surround;
			}
		}
	});
}
