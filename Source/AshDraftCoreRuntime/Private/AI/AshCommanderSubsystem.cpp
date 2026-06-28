// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshCommanderSubsystem.h"

#include "AI/AshGeneralSubsystem.h"
#include "AI/AshSquadSubsystem.h"
#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Character/AshGeneralCharacter.h"
#include "Engine/World.h"

namespace
{
	/**
	 * The shared strategic rule (ARCHITECTURE.md 8.1): from a position, pick the nearest base the team
	 * does not own to attack; if it owns everything, defend its nearest owned base; if there are no
	 * bases, hold position. Used identically for squads and for generals.
	 */
	void ComputeOrderForPosition(
		const TArray<AAshBaseActor*>& Bases, const FVector& From, EAshTeamId Team,
		EAshSquadOrder& OutOrder, FVector& OutObjective)
	{
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
			if (Base->GetOwningTeam() == Team)
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
			OutOrder = EAshSquadOrder::AttackBase;
			OutObjective = NearestHostileBase->GetActorLocation();
		}
		else if (NearestOwnedBase)
		{
			OutOrder = EAshSquadOrder::DefendBase;
			OutObjective = NearestOwnedBase->GetActorLocation();
		}
		else
		{
			OutOrder = EAshSquadOrder::None;
			OutObjective = From;
		}
	}
}

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
	UAshGeneralSubsystem* GeneralSubsystem = World ? World->GetSubsystem<UAshGeneralSubsystem>() : nullptr;

	// 1. Generals (Phase 22): the operational layer. Each general gets a strategic order from its own
	// position; its StateTree then executes it and drives its squad. Skipping general-owned squads in
	// step 2 hands those squads entirely to the general (it publishes their objective live).
	if (GeneralSubsystem)
	{
		for (int32 GeneralId : GeneralSubsystem->GetAllGeneralIds())
		{
			const FAshGeneralState GeneralState = GeneralSubsystem->GetGeneralState(GeneralId);
			const AAshGeneralCharacter* General = GeneralState.General.Get();
			const FVector From = General ? General->GetActorLocation() : GeneralState.ObjectiveLocation;

			EAshSquadOrder Order = EAshSquadOrder::None;
			FVector Objective = From;
			ComputeOrderForPosition(Bases, From, GeneralState.TeamId, Order, Objective);
			GeneralSubsystem->SetGeneralOrder(GeneralId, Order, Objective);
		}
	}

	// 2. Squads without a general (legacy placed spawners): the commander steers them directly, exactly
	// as before. Squads commanded by a general are skipped — the general owns their objective.
	for (int32 SquadId : SquadSubsystem->GetAllSquadIds())
	{
		if (GeneralSubsystem && GeneralSubsystem->IsSquadCommandedByGeneral(SquadId))
		{
			continue;
		}

		const FAshSquadState State = SquadSubsystem->GetSquadState(SquadId);
		const FVector From = State.AveragePosition;

		EAshSquadOrder Order = EAshSquadOrder::None;
		FVector Objective = From;
		ComputeOrderForPosition(Bases, From, State.TeamId, Order, Objective);
		SquadSubsystem->SetSquadOrder(SquadId, Order, Objective);
	}
}
