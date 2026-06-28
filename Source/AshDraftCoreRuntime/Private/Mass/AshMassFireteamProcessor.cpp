// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshMassFireteamProcessor.h"

#include "AI/AshFireteamSubsystem.h"
#include "AI/AshGeneralSubsystem.h"
#include "AI/AshSquadSubsystem.h"
#include "Character/AshGeneralCharacter.h"
#include "Mass/AshSoldierFragments.h"
#include "MassExecutionContext.h"
#include "Teams/AshTeamStatics.h"

namespace
{
	struct FAshRuntimeFireteam
	{
		int32 FireteamId = INDEX_NONE;
		int32 SquadId = INDEX_NONE;
		int32 FireteamIndexInSquad = 0;
		EAshTeamId TeamId = EAshTeamId::Neutral;
		FVector AveragePosition = FVector::ZeroVector;
		FVector LeaderPosition = FVector::ZeroVector;
		FMassEntityHandle LeaderEntity;
		TMap<int32, FMassEntityHandle> SlotEntities;
		int32 AliveCount = 0;
		int32 AssignedEnemyFireteamId = INDEX_NONE;
		TWeakObjectPtr<AActor> AssignedActorTarget;
	};

	FVector PlanarSafeNormal(const FVector& V, const FVector& Fallback = FVector::ForwardVector)
	{
		FVector P(V.X, V.Y, 0.f);
		return P.Normalize() ? P : Fallback;
	}

	FVector RotatePlanarOffset(const FVector& LocalOffset, const FVector& FacingDir)
	{
		const FRotator FacingRot = FacingDir.Rotation();
		return FRotationMatrix(FacingRot).TransformVector(LocalOffset);
	}
}

UAshMassFireteamProcessor::UAshMassFireteamProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteBefore.Add(TEXT("AshMassSoldierBehaviorProcessor"));
	ExecutionOrder.ExecuteBefore.Add(TEXT("AshMassMovementProcessor"));
	bRequiresGameThreadExecution = true;
}

void UAshMassFireteamProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FAshMovementFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshTeamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshSquadFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshFireteamFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAshFormationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FAshCombatTargetFragment>(EMassFragmentAccess::ReadWrite);
}

void UAshMassFireteamProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = Context.GetWorld();
	UAshFireteamSubsystem* FireteamSubsystem = World ? World->GetSubsystem<UAshFireteamSubsystem>() : nullptr;
	const UAshSquadSubsystem* SquadSubsystem = World ? World->GetSubsystem<UAshSquadSubsystem>() : nullptr;
	const UAshGeneralSubsystem* GeneralSubsystem = World ? World->GetSubsystem<UAshGeneralSubsystem>() : nullptr;

	TMap<int32, FAshRuntimeFireteam> Fireteams;

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshTeamFragment> TeamList = ChunkContext.GetFragmentView<FAshTeamFragment>();
		const TConstArrayView<FAshSquadFragment> SquadList = ChunkContext.GetFragmentView<FAshSquadFragment>();
		const TConstArrayView<FAshFireteamFragment> FireteamList = ChunkContext.GetFragmentView<FAshFireteamFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			if (HealthList[It].CurrentHealth <= 0.f || FireteamList[It].FireteamId == INDEX_NONE)
			{
				continue;
			}

			const FAshFireteamFragment& FireteamFrag = FireteamList[It];
			FAshRuntimeFireteam& Runtime = Fireteams.FindOrAdd(FireteamFrag.FireteamId);
			Runtime.FireteamId = FireteamFrag.FireteamId;
			Runtime.SquadId = SquadList[It].SquadId;
			Runtime.FireteamIndexInSquad = FireteamFrag.FireteamIndexInSquad;
			Runtime.TeamId = TeamList[It].TeamId;
			Runtime.AveragePosition += MovementList[It].Position;
			Runtime.SlotEntities.Add(FireteamFrag.SlotIndex, ChunkContext.GetEntity(It));
			Runtime.AliveCount++;

			if (FireteamFrag.Role == EAshFireteamRole::Leader)
			{
				Runtime.LeaderPosition = MovementList[It].Position;
				Runtime.LeaderEntity = ChunkContext.GetEntity(It);
			}
		}
	});

	for (TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
	{
		FAshRuntimeFireteam& Fireteam = Pair.Value;
		if (Fireteam.AliveCount > 0)
		{
			Fireteam.AveragePosition /= static_cast<float>(Fireteam.AliveCount);
			if (!Fireteam.LeaderEntity.IsSet())
			{
				Fireteam.LeaderPosition = Fireteam.AveragePosition;
			}
		}
	}

	for (TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
	{
		FAshRuntimeFireteam& Fireteam = Pair.Value;
		float BestSameIndexDistSq = FMath::Square(FireteamEngageRadius);
		float BestAnyDistSq = FMath::Square(FireteamEngageRadius);
		int32 BestSameIndexFireteamId = INDEX_NONE;
		int32 BestAnyFireteamId = INDEX_NONE;

		for (const TPair<int32, FAshRuntimeFireteam>& CandidatePair : Fireteams)
		{
			const FAshRuntimeFireteam& Other = CandidatePair.Value;
			if (Other.FireteamId == Fireteam.FireteamId
				|| Other.AliveCount <= 0
				|| !UAshTeamStatics::AreTeamsHostile(Fireteam.TeamId, Other.TeamId))
			{
				continue;
			}

			const float DistSq = FVector::DistSquared2D(Fireteam.AveragePosition, Other.AveragePosition);
			if (Other.FireteamIndexInSquad == Fireteam.FireteamIndexInSquad && DistSq < BestSameIndexDistSq)
			{
				BestSameIndexDistSq = DistSq;
				BestSameIndexFireteamId = Other.FireteamId;
			}
			if (DistSq < BestAnyDistSq)
			{
				BestAnyDistSq = DistSq;
				BestAnyFireteamId = Other.FireteamId;
			}
		}

		Fireteam.AssignedEnemyFireteamId = (BestSameIndexFireteamId != INDEX_NONE)
			? BestSameIndexFireteamId
			: BestAnyFireteamId;
		if (Fireteam.AssignedEnemyFireteamId != INDEX_NONE)
		{
			Fireteam.AssignedActorTarget = nullptr;
		}

		if (Fireteam.AssignedEnemyFireteamId == INDEX_NONE && GeneralSubsystem)
		{
			float BestActorDistSq = FMath::Square(GeneralActorEngageRadius);
			for (int32 GeneralId : GeneralSubsystem->GetAllGeneralIds())
			{
				const FAshGeneralState GeneralState = GeneralSubsystem->GetGeneralState(GeneralId);
				AAshGeneralCharacter* General = GeneralState.General.Get();
				if (!General || General->IsDead() || !UAshTeamStatics::AreTeamsHostile(Fireteam.TeamId, GeneralState.TeamId))
				{
					continue;
				}

				const float DistSq = FVector::DistSquared2D(Fireteam.AveragePosition, General->GetActorLocation());
				if (DistSq < BestActorDistSq)
				{
					BestActorDistSq = DistSq;
					Fireteam.AssignedActorTarget = General;
				}
			}
		}
	}

	if (FireteamSubsystem)
	{
		FireteamSubsystem->BeginFireteamUpdate();
		for (const TPair<int32, FAshRuntimeFireteam>& Pair : Fireteams)
		{
			const FAshRuntimeFireteam& Fireteam = Pair.Value;
			FAshFireteamState State;
			State.FireteamId = Fireteam.FireteamId;
			State.SquadId = Fireteam.SquadId;
			State.FireteamIndexInSquad = Fireteam.FireteamIndexInSquad;
			State.TeamId = Fireteam.TeamId;
			State.AveragePosition = Fireteam.AveragePosition;
			State.LeaderPosition = Fireteam.LeaderPosition;
			State.AliveCount = Fireteam.AliveCount;
			State.AssignedEnemyFireteamId = Fireteam.AssignedEnemyFireteamId;
			FireteamSubsystem->UpsertFireteam(State);
		}
	}

	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FAshMovementFragment> MovementList = ChunkContext.GetFragmentView<FAshMovementFragment>();
		const TConstArrayView<FAshHealthFragment> HealthList = ChunkContext.GetFragmentView<FAshHealthFragment>();
		const TConstArrayView<FAshSquadFragment> SquadList = ChunkContext.GetFragmentView<FAshSquadFragment>();
		const TConstArrayView<FAshFireteamFragment> FireteamList = ChunkContext.GetFragmentView<FAshFireteamFragment>();
		const TArrayView<FAshFormationFragment> FormationList = ChunkContext.GetMutableFragmentView<FAshFormationFragment>();
		const TArrayView<FAshCombatFragment> CombatList = ChunkContext.GetMutableFragmentView<FAshCombatFragment>();
		const TArrayView<FAshCombatTargetFragment> CombatTargetList = ChunkContext.GetMutableFragmentView<FAshCombatTargetFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			FAshFormationFragment& Formation = FormationList[It];
			FAshCombatFragment& Combat = CombatList[It];
			FAshCombatTargetFragment& CombatTarget = CombatTargetList[It];

			if (HealthList[It].CurrentHealth <= 0.f)
			{
				Formation.bHasDesiredPosition = false;
				Combat.Target.Reset();
				CombatTarget.TargetType = EAshCombatTargetType::None;
				CombatTarget.MassTarget.Reset();
				CombatTarget.ActorTarget = nullptr;
				continue;
			}

			const FAshFireteamFragment& FireteamFrag = FireteamList[It];
			const FAshRuntimeFireteam* Fireteam = Fireteams.Find(FireteamFrag.FireteamId);
			if (!Fireteam)
			{
				continue;
			}

			FVector Anchor = Fireteam->LeaderPosition;
			FVector Facing = FVector::ForwardVector;
			EAshFireteamState FireteamState = EAshFireteamState::Standby;

			const FAshRuntimeFireteam* EnemyFireteam = Fireteams.Find(Fireteam->AssignedEnemyFireteamId);
			const AActor* ActorTarget = Fireteam->AssignedActorTarget.Get();
			if (EnemyFireteam)
			{
				const FVector TargetPos = EnemyFireteam->AveragePosition;
				const FVector OwnSide = PlanarSafeNormal(Fireteam->AveragePosition - TargetPos, -FVector::ForwardVector);
				Anchor = TargetPos + OwnSide * CombatAnchorDistance;
				Facing = PlanarSafeNormal(TargetPos - Anchor, FVector::ForwardVector);
				FireteamState = EAshFireteamState::Engaged;

				const FMassEntityHandle* TargetSlot = EnemyFireteam->SlotEntities.Find(FireteamFrag.SlotIndex);
				if (!TargetSlot)
				{
					TargetSlot = EnemyFireteam->SlotEntities.Find(0);
				}
				Combat.Target = TargetSlot ? *TargetSlot : FMassEntityHandle();
				CombatTarget.TargetType = Combat.Target.IsSet() ? EAshCombatTargetType::MassEntity : EAshCombatTargetType::None;
				CombatTarget.MassTarget = Combat.Target;
				CombatTarget.ActorTarget = nullptr;
			}
			else if (ActorTarget)
			{
				const FVector TargetPos = ActorTarget->GetActorLocation();
				const FVector OwnSide = PlanarSafeNormal(Fireteam->AveragePosition - TargetPos, -FVector::ForwardVector);
				Anchor = TargetPos + OwnSide * CombatAnchorDistance;
				Facing = PlanarSafeNormal(TargetPos - Anchor, FVector::ForwardVector);
				FireteamState = EAshFireteamState::Engaged;
				Combat.Target.Reset();
				CombatTarget.TargetType = EAshCombatTargetType::Actor;
				CombatTarget.MassTarget.Reset();
				CombatTarget.ActorTarget = const_cast<AActor*>(ActorTarget);
			}
			else
			{
				FVector SquadObjective;
				float SquadFormationRadius = 0.f;
				if (SquadSubsystem && SquadSubsystem->GetSquadObjective(SquadList[It].SquadId, SquadObjective, SquadFormationRadius))
				{
					Facing = PlanarSafeNormal(SquadObjective - Fireteam->AveragePosition, FVector::ForwardVector);
					const int32 Column = (Fireteam->FireteamIndexInSquad % 3) - 1;
					const int32 Row = Fireteam->FireteamIndexInSquad / 3;
					const FVector TeamOffset(-Row * FireteamRowSpacing, Column * FireteamSpacing, 0.f);
					Anchor = SquadObjective + RotatePlanarOffset(TeamOffset, Facing);
					FireteamState = FVector::DistSquared2D(Fireteam->AveragePosition, Anchor) > FMath::Square(150.f)
						? EAshFireteamState::Moving
						: EAshFireteamState::Standby;
				}
				Combat.Target.Reset();
				CombatTarget.TargetType = EAshCombatTargetType::None;
				CombatTarget.MassTarget.Reset();
				CombatTarget.ActorTarget = nullptr;
			}

			Formation.FireteamState = FireteamState;
			Formation.DesiredWorldPosition = Anchor + RotatePlanarOffset(Formation.LocalOffset, Facing);
			Formation.bHasDesiredPosition = true;
		}
	});
}
