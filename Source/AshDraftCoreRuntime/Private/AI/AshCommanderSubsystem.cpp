// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshCommanderSubsystem.h"

#include "AI/AshSquadSubsystem.h"
#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Engine/World.h"

void UAshCommanderSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UAshBaseSubsystem* BaseSubsystem = InWorld.GetSubsystem<UAshBaseSubsystem>())
	{
		BaseSubsystem->OnAnyBaseOwnershipChanged.AddDynamic(this, &UAshCommanderSubsystem::HandleBaseOwnershipChanged);
		bBound = true;
	}

	// All actors have finished BeginPlay by now, so bases/squads are registered: assign once.
	ReevaluateOrders();
}

void UAshCommanderSubsystem::Deinitialize()
{
	if (bBound)
	{
		if (const UWorld* World = GetWorld())
		{
			if (UAshBaseSubsystem* BaseSubsystem = World->GetSubsystem<UAshBaseSubsystem>())
			{
				BaseSubsystem->OnAnyBaseOwnershipChanged.RemoveDynamic(this, &UAshCommanderSubsystem::HandleBaseOwnershipChanged);
			}
		}
		bBound = false;
	}

	Super::Deinitialize();
}

UAshSquadSubsystem* UAshCommanderSubsystem::GetSquadSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UAshSquadSubsystem>() : nullptr;
}

void UAshCommanderSubsystem::HandleBaseOwnershipChanged(AAshBaseActor* /*Base*/, EAshTeamId /*OldTeam*/, EAshTeamId /*NewTeam*/)
{
	// A base flipped: the whole strategic picture may have changed, so re-plan every squad.
	ReevaluateOrders();
}

void UAshCommanderSubsystem::ReevaluateOrders()
{
	const UWorld* World = GetWorld();
	UAshSquadSubsystem* SquadSubsystem = GetSquadSubsystem();
	UAshBaseSubsystem* BaseSubsystem = World ? World->GetSubsystem<UAshBaseSubsystem>() : nullptr;
	if (!SquadSubsystem || !BaseSubsystem)
	{
		return;
	}

	const TArray<AAshBaseActor*> Bases = BaseSubsystem->GetAllBases();

	for (int32 SquadId : SquadSubsystem->GetAllSquadIds())
	{
		const FAshSquadState State = SquadSubsystem->GetSquadState(SquadId);
		const FVector From = State.AveragePosition;

		// Find nearest base this team does NOT own (objective to take) and nearest owned base
		// (objective to defend), measured from the squad's current centre of mass.
		AAshBaseActor* NearestHostileBase = nullptr;
		AAshBaseActor* NearestOwnedBase = nullptr;
		float BestHostileDistSq = TNumericLimits<float>::Max();
		float BestOwnedDistSq = TNumericLimits<float>::Max();

		for (AAshBaseActor* Base : Bases)
		{
			if (!Base)
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(From, Base->GetActorLocation());
			if (Base->GetOwningTeam() == State.TeamId)
			{
				if (DistSq < BestOwnedDistSq)
				{
					BestOwnedDistSq = DistSq;
					NearestOwnedBase = Base;
				}
			}
			else if (DistSq < BestHostileDistSq)
			{
				BestHostileDistSq = DistSq;
				NearestHostileBase = Base;
			}
		}

		if (NearestHostileBase)
		{
			SquadSubsystem->SetSquadOrder(SquadId, EAshSquadOrder::AttackBase, NearestHostileBase->GetActorLocation());
		}
		else if (NearestOwnedBase)
		{
			SquadSubsystem->SetSquadOrder(SquadId, EAshSquadOrder::DefendBase, NearestOwnedBase->GetActorLocation());
		}
		else
		{
			// No bases at all: hold at the squad's own position.
			SquadSubsystem->SetSquadOrder(SquadId, EAshSquadOrder::None, From);
		}
	}
}
