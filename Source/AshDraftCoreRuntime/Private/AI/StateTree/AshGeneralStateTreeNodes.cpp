// Copyright YoosungHong. All Rights Reserved.

#include "AI/StateTree/AshGeneralStateTreeNodes.h"

#include "AIController.h"
#include "Character/AshGeneralCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

namespace
{
	/** Resolves the General being controlled from the AIController context. */
	AAshGeneralCharacter* GetGeneral(const FAshSTGeneralInstanceData& Inst)
	{
		return Inst.AIController ? Cast<AAshGeneralCharacter>(Inst.AIController->GetPawn()) : nullptr;
	}

	bool IsMoving(const AAIController* AI)
	{
		return AI && AI->GetMoveStatus() == EPathFollowingStatus::Moving;
	}

	float GetAcceptance(const AAshGeneralCharacter* General)
	{
		return (General && General->GetConfig()) ? General->GetConfig()->ObjectiveAcceptanceRadius : 400.f;
	}
}

// ===================================================================================================
// ExecuteOrder
// ===================================================================================================
EStateTreeRunStatus FAshSTTask_ExecuteOrder::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAshSTTask_ExecuteOrder::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Inst = Context.GetInstanceData(*this);
	AAshGeneralCharacter* General = GetGeneral(Inst);
	if (!General || General->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	// Self-yield to a higher-priority sub-objective: failing re-selects from the parent, which then
	// picks AttackTarget / AssaultStronghold (their enter conditions now pass).
	if (General->HasThreat(EAshThreatType::Enemy) || General->HasThreat(EAshThreatType::Stronghold))
	{
		return EStateTreeRunStatus::Failed;
	}

	// Troops follow the general (it leads them toward the objective; they stay within FormationRadius).
	General->PublishSquadObjective(General->GetActorLocation(), General->GetCommanderOrder());

	AAIController* AI = Inst.AIController;
	if (AI && General->HasCommanderObjective())
	{
		const FVector Objective = General->GetCommanderObjective();
		const float Acceptance = GetAcceptance(General);
		const float DistSq = FVector::DistSquared(General->GetActorLocation(), Objective);
		if (DistSq > FMath::Square(Acceptance))
		{
			if (!IsMoving(AI))
			{
				AI->MoveToLocation(Objective, Acceptance);
			}
		}
		else if (IsMoving(AI))
		{
			// Arrived: hold here (Defend) so the troops form up around the stationary general.
			AI->StopMovement();
		}
	}

	// The order is ongoing — never "succeeds"; it just keeps leading the troops.
	return EStateTreeRunStatus::Running;
}

// ===================================================================================================
// AttackTarget
// ===================================================================================================
EStateTreeRunStatus FAshSTTask_AttackTarget::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Inst = Context.GetInstanceData(*this);
	Inst.CurrentAttackTarget = nullptr;
	Inst.CurrentAttackTargetTime = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAshSTTask_AttackTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Inst = Context.GetInstanceData(*this);
	AAshGeneralCharacter* General = GetGeneral(Inst);
	if (!General || General->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Enemy = General->GetSensedEnemy();
	if (!Enemy)
	{
		// Threat resolved — succeed so the tree falls back to the commander order.
		return EStateTreeRunStatus::Succeeded;
	}

	if (Inst.CurrentAttackTarget != Enemy)
	{
		Inst.CurrentAttackTarget = Enemy;
		Inst.CurrentAttackTargetTime = 0.f;
	}
	else
	{
		Inst.CurrentAttackTargetTime += DeltaTime;
	}

	const UAshHeroConfig* Config = General->GetConfig();
	const float RetargetSeconds = Config ? Config->PlayerChaseRetargetSeconds : 5.f;
	if (RetargetSeconds > 0.f
		&& Inst.CurrentAttackTargetTime >= RetargetSeconds
		&& General->IsPlayerEnemyActor(Enemy))
	{
		const float RetargetRadius = Config ? Config->PlayerChaseRetargetRadius : 2200.f;
		if (General->TryRetargetSensedEnemy(Enemy, RetargetRadius))
		{
			Enemy = General->GetSensedEnemy();
			Inst.CurrentAttackTarget = Enemy;
			Inst.CurrentAttackTargetTime = 0.f;
		}
		else
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	// Troops converge into the fight itself, not just the general's back line.
	General->PublishCombatObjectiveNear(Enemy);

	AAIController* AI = Inst.AIController;
	if (AI)
	{
		AI->SetFocus(Enemy); // face the enemy while striking even when stopped
	}

	const float Range = General->GetAttackRange();
	const float DistSq = FVector::DistSquared(General->GetActorLocation(), Enemy->GetActorLocation());
	if (DistSq > FMath::Square(Range))
	{
		if (AI && !IsMoving(AI))
		{
			AI->MoveToActor(Enemy, Range * 0.8f);
		}
	}
	else
	{
		if (AI)
		{
			AI->StopMovement();
		}
		// The ability gates its own cooldown, so triggering each tick is safe.
		General->TriggerBasicAttack();
	}

	return EStateTreeRunStatus::Running;
}

void FAshSTTask_AttackTarget::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Inst = Context.GetInstanceData(*this);
	Inst.CurrentAttackTarget = nullptr;
	Inst.CurrentAttackTargetTime = 0.f;
	if (Inst.AIController)
	{
		Inst.AIController->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

// ===================================================================================================
// AssaultStronghold
// ===================================================================================================
EStateTreeRunStatus FAshSTTask_AssaultStronghold::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAshSTTask_AssaultStronghold::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Inst = Context.GetInstanceData(*this);
	AAshGeneralCharacter* General = GetGeneral(Inst);
	if (!General || General->IsDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	// A live enemy outranks a stronghold assault — yield so AttackTarget is selected.
	if (General->HasThreat(EAshThreatType::Enemy))
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Stronghold = General->GetSensedStronghold();
	if (!Stronghold)
	{
		// Captured / no longer hostile — resume the commander order.
		return EStateTreeRunStatus::Succeeded;
	}

	General->PublishSquadObjective(General->GetActorLocation(), EAshSquadOrder::AttackBase);

	AAIController* AI = Inst.AIController;
	if (AI)
	{
		const float Acceptance = GetAcceptance(General);
		const float DistSq = FVector::DistSquared(General->GetActorLocation(), Stronghold->GetActorLocation());
		if (DistSq > FMath::Square(Acceptance))
		{
			if (!IsMoving(AI))
			{
				AI->MoveToActor(Stronghold, Acceptance);
			}
		}
		else
		{
			AI->StopMovement();
		}
	}

	return EStateTreeRunStatus::Running;
}

// ===================================================================================================
// HasThreat condition
// ===================================================================================================
bool FAshSTCondition_HasThreat::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Inst = Context.GetInstanceData(*this);
	AAshGeneralCharacter* General = Inst.AIController ? Cast<AAshGeneralCharacter>(Inst.AIController->GetPawn()) : nullptr;
	return General && General->HasThreat(Inst.ThreatType);
}
