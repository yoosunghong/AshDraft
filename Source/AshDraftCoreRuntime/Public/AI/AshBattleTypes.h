// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Teams/AshTeamTypes.h"
#include "AshBattleTypes.generated.h"

/**
 * AshBattleTypes.h
 *
 * Plain data for the Global Combat layer (Phase 24, the "Engagement Director"). When two hostile
 * Generals encounter each other a Battle forms and UAshBattleSubsystem computes a *deliberate,
 * persistent* engagement plan: which enemy fireteam each fireteam fights and where on the battlefield
 * that duel takes place (a slot on a ring around the battle centre). This header declares only the
 * data the plan is made of — the algorithm lives in the subsystem, the execution in the Mass fireteam
 * processor (ARCHITECTURE.md 8 hierarchy: Commander -> General -> Battle/Squad -> Fireteam -> Soldier).
 */

/** Lifecycle of one Battle (a clash between two or more hostile Generals and their forces). */
UENUM(BlueprintType)
enum class EAshBattleState : uint8
{
	/** No battle: generals are out of encounter range, forces march under commander orders. */
	None		UMETA(DisplayName = "None"),

	/** Encounter just triggered: fireteams are being assigned + marching to their ring slots. */
	Forming		UMETA(DisplayName = "Forming"),

	/** Steady-state melee: the ring of squad-vs-squad duels is live. */
	Active		UMETA(DisplayName = "Active"),
};

/**
 * The plan for one fireteam inside an active Battle. Computed by UAshBattleSubsystem on its replan
 * cadence (not every frame) so it is *stable*: the fireteam commits to one designated enemy and one
 * deployment slot and marches there ignoring everything in between, instead of re-picking the nearest
 * body each frame (which produced the static grinding line). Consumed by UAshMassFireteamProcessor.
 */
USTRUCT(BlueprintType)
struct FAshEngagementAssignment
{
	GENERATED_BODY()

	/** True if this fireteam is committed to a battle duel this plan. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Battle")
	bool bValid = false;

	/** The enemy fireteam this fireteam is ordered to fight (its FAshFireteamFragment::FireteamId). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Battle")
	int32 EnemyFireteamId = INDEX_NONE;

	/**
	 * World-space deployment slot for this duel: a point on a ring around the battle centre. Both this
	 * fireteam and its designated enemy share the same slot, approaching from opposite sides, so they
	 * clash *there* rather than at their midpoint — spreading the battle into a circle of simultaneous
	 * duels (the Dynasty-Warriors melee) instead of one long line.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Battle")
	FVector RingSlot = FVector::ZeroVector;
};
