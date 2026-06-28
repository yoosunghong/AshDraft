// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassSoldierBehaviorProcessor.h"

#include "Engine/World.h"
#include "Mass/AshSoldierBehaviorConfig.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"
#include "Teams/AshTeamStatics.h"

namespace
{
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

		/** Nearest living entry hostile to SeekerTeam within MaxRange of From, excluding Self. */
		FMassEntityHandle FindNearestHostile(const FMassEntityHandle& Self, const FVector& From,
			EAshTeamId SeekerTeam, float MaxRange, FVector& OutTargetPos) const
		{
			const FIntPoint Base = CellOf(From);
			float BestDistSq = MaxRange * MaxRange;
			FMassEntityHandle Best;

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
						const float DistSq = FVector::DistSquared2D(From, E.Position);
						if (DistSq < BestDistSq)
						{
							BestDistSq = DistSq;
							Best = E.Handle;
							OutTargetPos = E.Position;
						}
					}
				}
			}
			return Best;
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

	// --- Pass A: snapshot every soldier into the sense grid. ---
	FAshSenseGrid Grid(MaxSense);
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			FAshSenseGrid::FEntry Entry;
			Entry.Handle = ChunkContext.GetEntity(It);
			Entry.Position = MovementList[It].Position;
			Entry.Team = TeamList[It].TeamId;
			Entry.bAlive = HealthList[It].CurrentHealth > 0.f;
			Grid.Add(Entry);
		}
	});

	// --- Pass B: per soldier, run the local state machine (throttled by AI LOD interval). ---
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshBehaviorFragment> BehaviorList = ChunkContext.GetFragmentView<FAshBehaviorFragment>();
		const TArrayView<FAshCombatFragment> CombatList = ChunkContext.GetMutableFragmentView<FAshCombatFragment>();
		const TArrayView<FAshCombatTargetFragment> CombatTargetList = ChunkContext.GetMutableFragmentView<FAshCombatTargetFragment>();
		const TArrayView<FAshSoldierStateFragment> StateList = ChunkContext.GetMutableFragmentView<FAshSoldierStateFragment>();
		const TArrayView<FAshLODFragment> LODList = ChunkContext.GetMutableFragmentView<FAshLODFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			// Dead soldiers hold no target/state; the death processor cleans them up.
			if (HealthList[It].CurrentHealth <= 0.f)
			{
				CombatList[It].Target.Reset();
				StateList[It].State = EAshSoldierState::FollowSquad;
				StateList[It].bEngaged = false;
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
			const float Sense = EffectiveSense(Cfg);
			const float MaxChase = Cfg ? Cfg->MaxLeashFromObjective : 1200.f;

			FAshCombatFragment& Combat = CombatList[It];
			FAshCombatTargetFragment& CombatTarget = CombatTargetList[It];
			FAshSoldierStateFragment& StateFrag = StateList[It];

			if (CombatTarget.TargetType == EAshCombatTargetType::Actor && CombatTarget.ActorTarget)
			{
				const float DistSq = FVector::DistSquared2D(SelfPos, CombatTarget.ActorTarget->GetActorLocation());
				if (!StateFrag.bEngaged)
				{
					StateFrag.EngageAnchor = SelfPos;
					StateFrag.bEngaged = true;
				}
				StateFrag.State = (DistSq <= FMath::Square(Combat.AttackRange))
					? EAshSoldierState::Attack
					: EAshSoldierState::Engage;
				Combat.Target.Reset();
				continue;
			}

			if (CombatTarget.TargetType == EAshCombatTargetType::MassEntity
				&& CombatTarget.MassTarget.IsSet()
				&& EntityManager.IsEntityValid(CombatTarget.MassTarget))
			{
				if (const FAshMovementFragment* TargetMovement = EntityManager.GetFragmentDataPtr<FAshMovementFragment>(CombatTarget.MassTarget))
				{
					const float DistSq = FVector::DistSquared2D(SelfPos, TargetMovement->Position);
					if (!StateFrag.bEngaged)
					{
						StateFrag.EngageAnchor = SelfPos;
						StateFrag.bEngaged = true;
					}
					StateFrag.State = (DistSq <= FMath::Square(Combat.AttackRange))
						? EAshSoldierState::Attack
						: EAshSoldierState::Engage;
					Combat.Target = CombatTarget.MassTarget;
					continue;
				}
			}

			// Sense the nearest hostile within the soldier's *local* radius. Deliberately small: a
			// soldier reacts only to enemies that cross its own path, never a map-wide search. An enemy
			// inside this radius is engaged immediately, so combat triggers on contact during the march.
			FVector TargetPos;
			const FMassEntityHandle Acquired =
				Grid.FindNearestHostile(Self, SelfPos, SelfTeam, Sense, TargetPos);

			if (!Acquired.IsSet())
			{
				// No local enemy: drop any target and rejoin the squad's flow-field advance.
				Combat.Target.Reset();
				StateFrag.State = EAshSoldierState::FollowSquad;
				StateFrag.bEngaged = false;
				continue;
			}

			// First contact: anchor here so the chase leash is measured from where we engaged.
			if (!StateFrag.bEngaged)
			{
				StateFrag.bEngaged = true;
				StateFrag.EngageAnchor = SelfPos;
			}

			// Chase leash: fight any enemy we sense, but pursue only MaxChase from the engage anchor;
			// past that, abandon the chase and let the flow field carry us back to the objective so
			// soldiers hold their line instead of wandering off after a straggler (0 = unlimited).
			const bool bWithinChase = (MaxChase <= 0.f)
				|| (FVector::DistSquared2D(StateFrag.EngageAnchor, SelfPos) <= FMath::Square(MaxChase));

			if (bWithinChase)
			{
				Combat.Target = Acquired;
				const float DistSq = FVector::DistSquared2D(SelfPos, TargetPos);
				StateFrag.State = (DistSq <= FMath::Square(Combat.AttackRange))
					? EAshSoldierState::Attack
					: EAshSoldierState::Engage;
			}
			else
			{
				// Strayed past the leash: stop pursuing and return to the advance. Keep the anchor so
				// the soldier doesn't immediately re-engage the same straggler from its new position.
				Combat.Target.Reset();
				StateFrag.State = EAshSoldierState::FollowSquad;
			}
		}
	});
}
