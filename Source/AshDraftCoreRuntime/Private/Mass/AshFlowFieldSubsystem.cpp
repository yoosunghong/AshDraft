// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshFlowFieldSubsystem.h"

#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "CollisionShape.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Mass/AshFlowFieldConfig.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

namespace
{
	/** Sentinel "impassable" cost; also the integration-field "unreached" value. */
	constexpr float AshFlowBlockedCost = TNumericLimits<float>::Max();

	/** 8-neighbour offsets and their step cost (orthogonal = 1, diagonal = sqrt(2)). */
	struct FAshNeighbour { int32 DX; int32 DY; float Cost; };
	const FAshNeighbour GAshNeighbours[8] =
	{
		{  1,  0, 1.0f }, { -1,  0, 1.0f }, {  0,  1, 1.0f }, {  0, -1, 1.0f },
		{  1,  1, 1.41421356f }, {  1, -1, 1.41421356f }, { -1,  1, 1.41421356f }, { -1, -1, 1.41421356f },
	};
}

float UAshFlowFieldSubsystem::GetCellSize() const
{
	return Config ? Config->CellSize : 500.f;
}

int32 UAshFlowFieldSubsystem::GetMaxCachedFields() const
{
	return Config ? Config->MaxCachedFields : 4;
}

void UAshFlowFieldSubsystem::InvalidateAll()
{
	FieldCache.Reset();
	bGridInitialized = false;
}

void UAshFlowFieldSubsystem::EnsureGridInitialized()
{
	if (bGridInitialized)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CellSize = GetCellSize();
	const float Margin = Config ? Config->GridMargin : 3000.f;
	const float DefaultHalfExtent = Config ? Config->DefaultGridHalfExtent : 15000.f;
	const int32 MaxCells = Config ? Config->MaxCellsPerAxis : 128;

	// Bound the grid to the battlefield: the AABB of all bases plus a margin so the army has
	// room to manoeuvre around objectives. With no bases, fall back to a box around origin.
	FVector Min(-DefaultHalfExtent, -DefaultHalfExtent, 0.f);
	FVector Max(DefaultHalfExtent, DefaultHalfExtent, 0.f);
	float PlaneZ = 0.f;

	if (const UAshBaseSubsystem* BaseSubsystem = World->GetSubsystem<UAshBaseSubsystem>())
	{
		bool bAnyBase = false;
		FVector AccMin(TNumericLimits<float>::Max());
		FVector AccMax(-TNumericLimits<float>::Max());
		float ZSum = 0.f;
		int32 ZCount = 0;
		for (const AAshBaseActor* Base : BaseSubsystem->GetAllBases())
		{
			if (!Base)
			{
				continue;
			}
			const FVector Loc = Base->GetActorLocation();
			AccMin = AccMin.ComponentMin(Loc);
			AccMax = AccMax.ComponentMax(Loc);
			ZSum += Loc.Z;
			++ZCount;
			bAnyBase = true;
		}
		if (bAnyBase)
		{
			Min = AccMin - FVector(Margin, Margin, 0.f);
			Max = AccMax + FVector(Margin, Margin, 0.f);
			PlaneZ = ZCount > 0 ? ZSum / ZCount : 0.f;
		}
	}

	GridOrigin = FVector(Min.X, Min.Y, PlaneZ);

	// Grow CellSize if the map would exceed the per-axis cell budget (keeps the solve bounded).
	const float WidthX = FMath::Max(CellSize, Max.X - Min.X);
	const float WidthY = FMath::Max(CellSize, Max.Y - Min.Y);
	const float NeededCell = FMath::Max(WidthX, WidthY) / FMath::Max(1, MaxCells);
	CellSize = FMath::Max(CellSize, NeededCell);

	CellsX = FMath::Clamp(FMath::CeilToInt(WidthX / CellSize), 1, MaxCells);
	CellsY = FMath::Clamp(FMath::CeilToInt(WidthY / CellSize), 1, MaxCells);

	// Static cost field: open everywhere (cost 1), optionally raising blocked cells to
	// impassable by sampling world geometry once (ARCHITECTURE.md 12.1: "compute simple cell cost").
	const int32 NumCells = CellsX * CellsY;
	CostField.Init(1.f, NumCells);

	// Sample ground height per cell once (debug draw places arrows above the real surface) and,
	// when enabled, mark cells blocked by static geometry. Both are one-time, grid-bounded.
	CellGroundZ.Init(GridOrigin.Z, NumCells);
	const bool bSampleObstacles = Config && Config->bSampleObstacles;
	const float ProbeRadius = bSampleObstacles ? CellSize * Config->ObstacleProbeFraction : 0.f;
	const FCollisionShape Probe = FCollisionShape::MakeSphere(ProbeRadius);
	FCollisionQueryParams Params(TEXT("AshFlowFieldGrid"), false);

	for (int32 Y = 0; Y < CellsY; ++Y)
	{
		for (int32 X = 0; X < CellsX; ++X)
		{
			const int32 Index = CellIndex(X, Y);
			const FVector Center = CellCenter(X, Y);

			// Downward trace to find the walkable surface height for visualisation.
			const FVector TraceStart(Center.X, Center.Y, GridOrigin.Z + 50000.f);
			const FVector TraceEnd(Center.X, Center.Y, GridOrigin.Z - 50000.f);
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
			{
				CellGroundZ[Index] = Hit.ImpactPoint.Z;
			}

			if (bSampleObstacles && World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_WorldStatic, Probe, Params))
			{
				CostField[Index] = AshFlowBlockedCost;
			}
		}
	}

	FieldCache.Reset();
	bGridInitialized = true;
}

bool UAshFlowFieldSubsystem::WorldToCell(const FVector& World, int32& OutX, int32& OutY) const
{
	const float LocalX = (World.X - GridOrigin.X) / CellSize;
	const float LocalY = (World.Y - GridOrigin.Y) / CellSize;
	const int32 RawX = FMath::FloorToInt(LocalX);
	const int32 RawY = FMath::FloorToInt(LocalY);
	OutX = FMath::Clamp(RawX, 0, CellsX - 1);
	OutY = FMath::Clamp(RawY, 0, CellsY - 1);
	return RawX >= 0 && RawX < CellsX && RawY >= 0 && RawY < CellsY;
}

FVector UAshFlowFieldSubsystem::CellCenter(int32 X, int32 Y) const
{
	return FVector(
		GridOrigin.X + (X + 0.5f) * CellSize,
		GridOrigin.Y + (Y + 0.5f) * CellSize,
		GridOrigin.Z);
}

void UAshFlowFieldSubsystem::BuildIntegrationField(int32 GoalCellIndex, TArray<float>& OutIntegration) const
{
	const int32 NumCells = CellsX * CellsY;
	OutIntegration.Init(AshFlowBlockedCost, NumCells);
	if (!CostField.IsValidIndex(GoalCellIndex) || CostField[GoalCellIndex] >= AshFlowBlockedCost)
	{
		return;
	}

	// Label-correcting wavefront (SPFA): a cell is re-queued whenever a cheaper route to the
	// goal is found. Costs are near-uniform so this settles quickly on a PoC-sized grid.
	OutIntegration[GoalCellIndex] = 0.f;
	TArray<int32> Queue;
	Queue.Reserve(NumCells);
	Queue.Add(GoalCellIndex);
	int32 Head = 0;

	while (Head < Queue.Num())
	{
		const int32 Current = Queue[Head++];
		const int32 CX = Current % CellsX;
		const int32 CY = Current / CellsX;
		const float CurrentCost = OutIntegration[Current];

		for (const FAshNeighbour& N : GAshNeighbours)
		{
			const int32 NX = CX + N.DX;
			const int32 NY = CY + N.DY;
			if (NX < 0 || NX >= CellsX || NY < 0 || NY >= CellsY)
			{
				continue;
			}
			const int32 NIndex = CellIndex(NX, NY);
			if (CostField[NIndex] >= AshFlowBlockedCost)
			{
				continue; // impassable
			}
			// Don't let diagonals cut through a blocked corner.
			if (N.DX != 0 && N.DY != 0)
			{
				if (CostField[CellIndex(CX + N.DX, CY)] >= AshFlowBlockedCost ||
					CostField[CellIndex(CX, CY + N.DY)] >= AshFlowBlockedCost)
				{
					continue;
				}
			}
			const float Candidate = CurrentCost + N.Cost * CostField[NIndex];
			if (Candidate < OutIntegration[NIndex])
			{
				OutIntegration[NIndex] = Candidate;
				Queue.Add(NIndex);
			}
		}
	}
}

void UAshFlowFieldSubsystem::BuildFlowFromIntegration(const TArray<float>& Integration, TArray<FVector2D>& OutFlow) const
{
	const int32 NumCells = CellsX * CellsY;
	OutFlow.Init(FVector2D::ZeroVector, NumCells);

	for (int32 Y = 0; Y < CellsY; ++Y)
	{
		for (int32 X = 0; X < CellsX; ++X)
		{
			const int32 Index = CellIndex(X, Y);
			if (Integration[Index] >= AshFlowBlockedCost)
			{
				continue; // blocked / unreachable -> no flow
			}

			float BestCost = Integration[Index];
			int32 BestDX = 0;
			int32 BestDY = 0;
			for (const FAshNeighbour& N : GAshNeighbours)
			{
				const int32 NX = X + N.DX;
				const int32 NY = Y + N.DY;
				if (NX < 0 || NX >= CellsX || NY < 0 || NY >= CellsY)
				{
					continue;
				}
				const float NeighbourCost = Integration[CellIndex(NX, NY)];
				if (NeighbourCost < BestCost)
				{
					BestCost = NeighbourCost;
					BestDX = N.DX;
					BestDY = N.DY;
				}
			}
			if (BestDX != 0 || BestDY != 0)
			{
				OutFlow[Index] = FVector2D((float)BestDX, (float)BestDY).GetSafeNormal();
			}
		}
	}
}

const TArray<FVector2D>& UAshFlowFieldSubsystem::EnsureFieldForGoal(int32 GoalCellIndex)
{
	if (const TArray<FVector2D>* Existing = FieldCache.Find(GoalCellIndex))
	{
		return *Existing;
	}

	// Bound the cache: a handful of target bases at most. Bases are static so a flushed
	// field re-solves identically on next request.
	if (FieldCache.Num() >= GetMaxCachedFields())
	{
		FieldCache.Reset();
	}

	TArray<float> Integration;
	BuildIntegrationField(GoalCellIndex, Integration);

	TArray<FVector2D>& Flow = FieldCache.Add(GoalCellIndex);
	BuildFlowFromIntegration(Integration, Flow);
	return Flow;
}

bool UAshFlowFieldSubsystem::GetFlowDirection(const FVector& GoalLocation, const FVector& FromLocation, FVector& OutDirection)
{
	EnsureGridInitialized();
	if (!bGridInitialized || CellsX <= 0 || CellsY <= 0)
	{
		return false;
	}

	int32 GoalX, GoalY, FromX, FromY;
	WorldToCell(GoalLocation, GoalX, GoalY);
	const bool bFromInGrid = WorldToCell(FromLocation, FromX, FromY);

	const TArray<FVector2D>& Flow = EnsureFieldForGoal(CellIndex(GoalX, GoalY));

	// Off-grid soldiers (or a cell with no downhill flow, e.g. the goal cell itself) steer
	// straight at the goal so they always make progress toward / onto the field.
	const int32 FromIndex = CellIndex(FromX, FromY);
	if (bFromInGrid && Flow.IsValidIndex(FromIndex) && !Flow[FromIndex].IsNearlyZero())
	{
		const FVector2D& Dir2D = Flow[FromIndex];
		OutDirection = FVector(Dir2D.X, Dir2D.Y, 0.f);
		return true;
	}

	const FVector ToGoal = (GoalLocation - FromLocation);
	OutDirection = ToGoal.GetSafeNormal2D();
	return !OutDirection.IsNearlyZero();
}

void UAshFlowFieldSubsystem::DrawDebug(const UWorld* World) const
{
#if ENABLE_DRAW_DEBUG
	if (!World || !bGridInitialized)
	{
		return;
	}

	const float ArrowLen = CellSize * 0.4f;
	for (const TPair<int32, TArray<FVector2D>>& Pair : FieldCache)
	{
		const TArray<FVector2D>& Flow = Pair.Value;
		const int32 GoalX = Pair.Key % CellsX;
		const int32 GoalY = Pair.Key / CellsX;

		// Lift arrows above the sampled ground so they clear the floor regardless of where the
		// grid plane (average base height) sits relative to the terrain.
		const float DrawZOffset = 60.f;
		for (int32 Y = 0; Y < CellsY; ++Y)
		{
			for (int32 X = 0; X < CellsX; ++X)
			{
				const int32 Index = CellIndex(X, Y);
				const float GroundZ = CellGroundZ.IsValidIndex(Index) ? CellGroundZ[Index] : GridOrigin.Z;
				FVector Center = CellCenter(X, Y);
				Center.Z = GroundZ + DrawZOffset;
				const FVector2D& Dir = Flow.IsValidIndex(Index) ? Flow[Index] : FVector2D::ZeroVector;
				if (CostField.IsValidIndex(Index) && CostField[Index] >= AshFlowBlockedCost)
				{
					DrawDebugPoint(World, Center, 8.f, FColor::Black, false, -1.f); // blocked cell
				}
				else if (!Dir.IsNearlyZero())
				{
					const FVector End = Center + FVector(Dir.X, Dir.Y, 0.f) * ArrowLen;
					DrawDebugDirectionalArrow(World, Center, End, 40.f, FColor::Cyan, false, -1.f, 0, 2.f);
				}
			}
		}
		const float GoalGroundZ = CellGroundZ.IsValidIndex(Pair.Key) ? CellGroundZ[Pair.Key] : GridOrigin.Z;
		FVector GoalCenter = CellCenter(GoalX, GoalY);
		GoalCenter.Z = GoalGroundZ + DrawZOffset;
		DrawDebugSphere(World, GoalCenter, CellSize * 0.5f, 12, FColor::Green, false, -1.f, 0, 3.f);
	}
#endif
}
