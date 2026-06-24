// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassCombatProcessor.h"

#include "Engine/World.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"
#include "Teams/AshTeamStatics.h"

namespace
{
	/**
	 * Minimal uniform 2D spatial hash used only for soldier-vs-soldier target acquisition.
	 * Cells are AcquisitionRadius wide so a 3x3 cell neighbourhood always covers the search
	 * radius. File-local on purpose: it is an implementation detail of the combat pass.
	 */
	struct FAshCombatGrid
	{
		struct FEntry
		{
			FMassEntityHandle Handle;
			FVector Position = FVector::ZeroVector;
			EAshTeamId Team = EAshTeamId::Neutral;
			bool bAlive = false;
		};

		explicit FAshCombatGrid(float InCellSize)
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
			const float MaxRangeSq = MaxRange * MaxRange;
			float BestDistSq = MaxRangeSq;
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

UAshMassCombatProcessor::UAshMassCombatProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Reads team statics + writes other entities' health via the entity manager: game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassCombatProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatEventFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshLODFragment>(EMassFragmentAccess::ReadWrite);
}

void UAshMassCombatProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = Context.GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;

	// --- Pass A: snapshot every soldier into the spatial grid. ---
	FAshCombatGrid Grid(AcquisitionRadius);
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			FAshCombatGrid::FEntry Entry;
			Entry.Handle = ChunkContext.GetEntity(It);
			Entry.Position = MovementList[It].Position;
			Entry.Team = TeamList[It].TeamId;
			Entry.bAlive = HealthList[It].CurrentHealth > 0.f;
			Grid.Add(Entry);
		}
	});

	// --- Pass B: acquire targets and apply damage (throttled by AI LOD interval). ---
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TArrayView<FAshCombatFragment> CombatList = ChunkContext.GetMutableFragmentView<FAshCombatFragment>();
		const TArrayView<FAshCombatEventFragment> EventList = ChunkContext.GetMutableFragmentView<FAshCombatEventFragment>();
		const TArrayView<FAshLODFragment> LODList = ChunkContext.GetMutableFragmentView<FAshLODFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			// Dead soldiers do nothing; the death processor will clean them up.
			if (HealthList[It].CurrentHealth <= 0.f)
			{
				continue;
			}

			FAshLODFragment& LOD = LODList[It];
			const float Elapsed = WorldTime - LOD.LastUpdateTime;
			// AI LOD time slicing: only run the expensive combat tick once per UpdateInterval.
			if (Elapsed < LOD.UpdateInterval)
			{
				continue;
			}
			LOD.LastUpdateTime = WorldTime;

			FAshCombatFragment& Combat = CombatList[It];
			Combat.TimeSinceLastAttack += Elapsed;

			const FMassEntityHandle Self = ChunkContext.GetEntity(It);
			const FVector SelfPos = MovementList[It].Position;
			const EAshTeamId SelfTeam = TeamList[It].TeamId;

			// Acquire / refresh target. Drop a target that died or moved out of range.
			FVector TargetPos;
			const FMassEntityHandle Acquired = Grid.FindNearestHostile(Self, SelfPos, SelfTeam, AcquisitionRadius, TargetPos);
			Combat.Target = Acquired;

			if (!Acquired.IsSet() || !EntityManager.IsEntityValid(Acquired))
			{
				continue;
			}

			// Strike when in range and off cooldown.
			const float DistSq = FVector::DistSquared2D(SelfPos, TargetPos);
			if (DistSq <= FMath::Square(Combat.AttackRange) && Combat.TimeSinceLastAttack >= Combat.AttackCooldown)
			{
				if (FAshHealthFragment* TargetHealth = EntityManager.GetFragmentDataPtr<FAshHealthFragment>(Acquired))
				{
					TargetHealth->CurrentHealth = FMath::Max(0.f, TargetHealth->CurrentHealth - Combat.AttackPower);
					Combat.TimeSinceLastAttack = 0.f;

					// Surface one-shot animation events for the representation layer (Phase 15):
					// the attacker plays its attack montage, the victim its hit-react montage.
					EventList[It].bAttackedThisTick = true;
					if (FAshCombatEventFragment* TargetEvent = EntityManager.GetFragmentDataPtr<FAshCombatEventFragment>(Acquired))
					{
						TargetEvent->bWasHitThisTick = true;
					}
				}
			}
		}
	});
}
