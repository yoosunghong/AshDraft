// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/AshSquadTypes.h"
#include "Teams/AshTeamTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "AshGeneralTypes.generated.h"

class AAshGeneralCharacter;

/**
 * AshGeneralTypes.h
 *
 * Shared data for the Phase 22 operational AI layer (the General). Plain data only — the General's
 * behavior lives in its StateTree + AAshGeneralCharacter; this header just declares the registry
 * record and the threat classification used by the StateTree conditions.
 */

/**
 * Which kind of higher-priority sub-objective a StateTree condition is asking about (Phase 22). A
 * single condition node (FAshSTCondition_HasThreat) takes one of these so the same C++ node serves
 * both the "enemy encountered" and "enemy stronghold in the path" preemptions, selected as data.
 */
UENUM(BlueprintType)
enum class EAshThreatType : uint8
{
	/** A hostile actor (enemy general / hero / soldier proxy) sensed within EnemyEngageRadius. */
	Enemy		UMETA(DisplayName = "Enemy In Range"),

	/** A hostile base sensed within StrongholdDetourRadius of the general / its path. */
	Stronghold	UMETA(DisplayName = "Stronghold In Path"),
};

/**
 * Registry record for one General held by UAshGeneralSubsystem. Mirrors FAshSquadState: the General
 * owns no soldiers directly — it owns a SquadId, and the commander reasons over this record to hand
 * the General a strategic order, exactly as it does for squads (ARCHITECTURE.md 8.1). The General
 * actor itself is the source of truth for live position / sensing; this is the strategic seam.
 */
USTRUCT(BlueprintType)
struct FAshGeneralState
{
	GENERATED_BODY()

	/** Unique general id (assigned on registration). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|General")
	int32 GeneralId = INDEX_NONE;

	/** The general actor (weak: the subsystem never owns the general's lifetime; 18.4). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|General")
	TWeakObjectPtr<AAshGeneralCharacter> General;

	/** Team this general fights for. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|General")
	EAshTeamId TeamId = EAshTeamId::Neutral;

	/** The squad this general commands (its troops carry this id on FAshSquadFragment). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|General")
	int32 SquadId = INDEX_NONE;

	/** Strategic order from the commander (reuses the squad order vocabulary). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|General")
	EAshSquadOrder Order = EAshSquadOrder::None;

	/** World objective the order points at (target/owned base). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|General")
	FVector ObjectiveLocation = FVector::ZeroVector;

	/** True once the commander has assigned an objective. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|General")
	bool bHasObjective = false;

	/** Current actor AI-LOD level (0 = near/high think-rate .. 3 = far/abstract). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|General")
	int32 LODLevel = 0;
};
