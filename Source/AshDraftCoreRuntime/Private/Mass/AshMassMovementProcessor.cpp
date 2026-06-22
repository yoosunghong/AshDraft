// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassMovementProcessor.h"

#include "AI/AshSquadSubsystem.h"
#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Engine/World.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"

namespace
{
	/** Resolves the nearest base not owned by SoldierTeam (enemy or neutral). Returns false if none. */
	bool FindNearestCapturableBase(const UWorld* World, EAshTeamId SoldierTeam, const FVector& From, FVector& OutLocation)
	{
		const UAshBaseSubsystem* BaseSubsystem = World ? World->GetSubsystem<UAshBaseSubsystem>() : nullptr;
		if (!BaseSubsystem)
		{
			return false;
		}

		float BestDistSq = TNumericLimits<float>::Max();
		bool bFound = false;
		for (const AAshBaseActor* Base : BaseSubsystem->GetAllBases())
		{
			if (!Base || Base->GetOwningTeam() == SoldierTeam)
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(From, Base->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutLocation = Base->GetActorLocation();
				bFound = true;
			}
		}
		return bFound;
	}
}

UAshMassMovementProcessor::UAshMassMovementProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	// Reads the squad subsystem and base actors on the game thread.
	bRequiresGameThreadExecution = true;
}

void UAshMassMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshSquadFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
}

void UAshMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaTime = Context.GetDeltaTimeSeconds();
	const UWorld* World = Context.GetWorld();
	const UAshSquadSubsystem* SquadSubsystem = World ? World->GetSubsystem<UAshSquadSubsystem>() : nullptr;
	const float ArrivalToleranceSq = ArrivalTolerance * ArrivalTolerance;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FAshMovementFragment> MovementList = ChunkContext.GetMutableFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshCombatFragment> CombatList = ChunkContext.GetFragmentView<FAshCombatFragment>();
		const TConstArrayView<FAshSquadFragment> SquadList = ChunkContext.GetFragmentView<FAshSquadFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			FAshMovementFragment& Movement = MovementList[It];
			const FAshCombatFragment& Combat = CombatList[It];
			const FAshSquadFragment& Squad = SquadList[It];

			// 1. Resolve a destination, in priority order.
			FVector Destination = Movement.Position;
			bool bHasDestination = false;
			float StopDistance = ArrivalTolerance;

			if (Combat.Target.IsSet() && EntityManager.IsEntityValid(Combat.Target))
			{
				if (const FAshMovementFragment* TargetMovement = EntityManager.GetFragmentDataPtr<FAshMovementFragment>(Combat.Target))
				{
					Destination = TargetMovement->Position;
					bHasDestination = true;
					// Stop just inside attack range so the combat processor can strike.
					StopDistance = FMath::Max(ArrivalTolerance, Combat.AttackRange * 0.9f);
				}
			}

			if (!bHasDestination && SquadSubsystem)
			{
				FVector SquadObjective;
				if (SquadSubsystem->GetSquadObjective(Squad.SquadId, SquadObjective))
				{
					Destination = SquadObjective;
					bHasDestination = true;
				}
			}

			if (!bHasDestination)
			{
				FVector BaseLocation;
				if (FindNearestCapturableBase(World, TeamList[It].TeamId, Movement.Position, BaseLocation))
				{
					Destination = BaseLocation;
					bHasDestination = true;
				}
			}

			// 2. Steer + integrate.
			if (bHasDestination)
			{
				const FVector ToDest = Destination - Movement.Position;
				const float DistSq = ToDest.SizeSquared2D();
				if (DistSq > FMath::Square(StopDistance) && DistSq > ArrivalToleranceSq)
				{
					const FVector Dir = ToDest.GetSafeNormal2D();
					Movement.Velocity = Dir * Movement.MoveSpeed;
				}
				else
				{
					Movement.Velocity = FVector::ZeroVector;
				}
			}
			else
			{
				Movement.Velocity = FVector::ZeroVector;
			}

			Movement.Position += Movement.Velocity * DeltaTime;
		}
	});
}
