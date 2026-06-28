// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "AI/AshGeneralTypes.h"
#include "AshGeneralStateTreeNodes.generated.h"

class AAIController;
class AActor;

/**
 * AshGeneralStateTreeNodes.h
 *
 * The C++ surface of the General's StateTree (Phase 22). These four nodes are the building blocks the
 * editor wires into ST_AshGeneral; the asset only arranges them, so all behavior lives here.
 *
 * They are grouped in one header/cpp because StateTree nodes are small, tightly-related data structs
 * (the project already groups trivial structs like the Mass fragments this way) — the one-class-per-
 * file rule (CLAUDE.md 18.2) targets substantial classes.
 *
 * Priority model (no complex editor transitions needed): the three task states are siblings under one
 * parent, ordered AttackTarget > AssaultStronghold > ExecuteOrder, each gated by a HasThreat enter
 * condition (ExecuteOrder has none). Each task *self-yields* (returns Failed) the moment a
 * higher-priority premise appears, so on the next selection the parent re-picks the first sibling
 * whose condition passes. That is what lets a higher-priority objective (an enemy, a stronghold in the
 * path) preempt the commander order and then fall back to it — exactly the General brief.
 *
 * All nodes resolve the General from the AIController context (auto-bound by the AI schema) → GetPawn().
 * Every task republishes the squad objective = the General's own location each tick, so the troops
 * always follow the General wherever its current (sub-)objective takes it.
 */

class AAshGeneralCharacter;

// ---------------------------------------------------------------------------------------------------
// Shared instance data: just the AIController context (the AI schema binds it automatically).
// ---------------------------------------------------------------------------------------------------
USTRUCT()
struct FAshSTGeneralInstanceData
{
	GENERATED_BODY()

	/** Controlled by the AI schema's "AIController" context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> CurrentAttackTarget = nullptr;

	UPROPERTY()
	float CurrentAttackTargetTime = 0.f;
};

// ---------------------------------------------------------------------------------------------------
// ExecuteOrder — the default / lowest-priority state: carry out the commander's order.
// ---------------------------------------------------------------------------------------------------
USTRUCT(meta = (DisplayName = "Ash General: Execute Order", Category = "Ash|General"))
struct ASHDRAFTCORERUNTIME_API FAshSTTask_ExecuteOrder : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshSTGeneralInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// ---------------------------------------------------------------------------------------------------
// AttackTarget — highest priority: engage a sensed hostile actor, then resume the order.
// ---------------------------------------------------------------------------------------------------
USTRUCT(meta = (DisplayName = "Ash General: Attack Target", Category = "Ash|General"))
struct ASHDRAFTCORERUNTIME_API FAshSTTask_AttackTarget : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshSTGeneralInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

// ---------------------------------------------------------------------------------------------------
// AssaultStronghold — mid priority: contest a hostile base in the path (yields to AttackTarget).
// ---------------------------------------------------------------------------------------------------
USTRUCT(meta = (DisplayName = "Ash General: Assault Stronghold", Category = "Ash|General"))
struct ASHDRAFTCORERUNTIME_API FAshSTTask_AssaultStronghold : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshSTGeneralInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// ---------------------------------------------------------------------------------------------------
// HasThreat — enter condition selecting which sub-objective applies (data-driven by ThreatType).
// ---------------------------------------------------------------------------------------------------
USTRUCT()
struct FAshSTConditionInstanceData
{
	GENERATED_BODY()

	/** Controlled by the AI schema's "AIController" context. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAIController> AIController = nullptr;

	/** Which threat this condition tests for (Enemy in range / Stronghold in path). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	EAshThreatType ThreatType = EAshThreatType::Enemy;
};

USTRUCT(meta = (DisplayName = "Ash General: Has Threat", Category = "Ash|General"))
struct ASHDRAFTCORERUNTIME_API FAshSTCondition_HasThreat : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshSTConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
