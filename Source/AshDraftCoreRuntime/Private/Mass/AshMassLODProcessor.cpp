// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassLODProcessor.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mass/AshMassLODConfig.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"
#include "Performance/AshPerfStatics.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

UAshMassLODProcessor::UAshMassLODProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// LOD should be decided before combat consumes the update interval.
	ExecutionOrder.ExecuteBefore.Add(TEXT("AshMassCombatProcessor"));
	// Resolves the player pawn + optional debug draw: game thread.
	bRequiresGameThreadExecution = true;

	// Default per-LOD update cadence (ARCHITECTURE.md 9.3): near soldiers update often,
	// far soldiers rarely.
	LODUpdateIntervals[0] = 0.05f;
	LODUpdateIntervals[1] = 0.15f;
	LODUpdateIntervals[2] = 0.5f;
	LODUpdateIntervals[3] = 1.0f;
}

void UAshMassLODProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshLODFragment>(EMassFragmentAccess::ReadWrite);
}

void UAshMassLODProcessor::ResolveConfig()
{
	if (bConfigResolved)
	{
		return;
	}
	bConfigResolved = true;

	// Synchronous load is acceptable: this runs once, and the config is a tiny UDataAsset.
	if (const UAshMassLODConfig* Config = LODConfig.LoadSynchronous())
	{
		LOD0MaxDistance = Config->LOD0MaxDistance;
		LOD1MaxDistance = Config->LOD1MaxDistance;
		LOD2MaxDistance = Config->LOD2MaxDistance;
		for (int32 i = 0; i < 4; ++i)
		{
			LODUpdateIntervals[i] = Config->LODUpdateIntervals[i];
		}
		NumTimeSliceBatches = FMath::Max(1, Config->NumTimeSliceBatches);
	}
}

int32 UAshMassLODProcessor::ComputeLODLevel(float Distance) const
{
	if (Distance <= LOD0MaxDistance) { return 0; }
	if (Distance <= LOD1MaxDistance) { return 1; }
	if (Distance <= LOD2MaxDistance) { return 2; }
	return 3;
}

void UAshMassLODProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}

	// Apply the optional data-asset overrides once (no-op when no config is assigned).
	ResolveConfig();

	// LOD reference point: the local player pawn. Resolved softly (no hard hero dependency,
	// ARCHITECTURE.md 18.4); when absent every soldier falls back to the farthest LOD.
	FVector PlayerLocation = FVector::ZeroVector;
	bool bHasPlayer = false;
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			PlayerLocation = Pawn->GetActorLocation();
			bHasPlayer = true;
		}
	}

	// Phase 18 toggles: disabling LOD forces every soldier to full fidelity; disabling time
	// slicing collapses to one batch so the whole population is re-evaluated each frame.
	const bool bLODEnabled = AshPerf::IsLODEnabled();
	const int32 BatchCount = (bLODEnabled && AshPerf::IsTimeSlicingEnabled()) ? FMath::Max(1, NumTimeSliceBatches) : 1;
	const int32 ActiveBatch = FrameCounter % BatchCount;
	++FrameCounter;

	int32 GlobalIndex = 0;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TArrayView<FAshLODFragment> LODList = ChunkContext.GetMutableFragmentView<FAshLODFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const int32 ThisIndex = GlobalIndex++;
			FAshLODFragment& LOD = LODList[It];

			if (!bLODEnabled)
			{
				// LOD disabled (Phase 18 baseline): everyone is LOD 0 and never throttled, so the
				// combat processor (which gates on UpdateInterval) runs full-rate for all soldiers.
				LOD.LODLevel = 0;
				LOD.UpdateInterval = 0.f;
			}
			// Time slicing: only re-evaluate this soldier's LOD on its assigned frame
			// (BatchCount collapses to 1 when time slicing is disabled, so this is always true).
			else if ((ThisIndex % BatchCount) == ActiveBatch)
			{
				const float Distance = bHasPlayer
					? FVector::Dist(PlayerLocation, MovementList[It].Position)
					: TNumericLimits<float>::Max();
				const int32 NewLevel = ComputeLODLevel(Distance);
				LOD.LODLevel = NewLevel;
				LOD.UpdateInterval = LODUpdateIntervals[FMath::Clamp(NewLevel, 0, 3)];
			}

#if ENABLE_DRAW_DEBUG
			if (bDrawLODDebug)
			{
				static const FColor LODColors[4] = { FColor::Green, FColor::Yellow, FColor::Orange, FColor::Red };
				const FColor Color = LODColors[FMath::Clamp(LOD.LODLevel, 0, 3)];
				DrawDebugPoint(World, MovementList[It].Position + FVector(0, 0, 50.f), 10.f, Color, false, -1.f);
			}
#endif
		}
	});
}
