// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshSoldierConfig.generated.h"

/**
 * UAshSoldierConfig
 *
 * Data-driven archetype for the Phase 8 prototype soldier (ARCHITECTURE.md 17 / 7.4).
 * This is the "basic soldier data model": health, movement, and combat tunables that
 * must not be hardcoded on the soldier actor. A placed/spawned AAshSoldierCharacter
 * references one of these so squads of soldiers can be tuned without recompiling, and
 * so the same numbers map cleanly onto the FAshHealthFragment / FAshMovementFragment /
 * FAshCombatFragment data the Mass implementation (Phase 9-10) will adopt.
 *
 * The soldier falls back to inline defaults when no config is assigned.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshSoldierConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Full (and initial) health. Maps to FAshHealthFragment::MaxHealth later. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 50.f;

	/** Ground movement speed (cm/s). Maps to FAshMovementFragment::MoveSpeed later. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 350.f;

	/** Distance (cm) within which the soldier stops advancing and may attack a target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 150.f;

	/** Damage applied to the target per successful attack. Maps to FAshCombatFragment::AttackPower. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.f;

	/** Seconds between attacks. Maps to FAshCombatFragment::AttackCooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Combat", meta = (ClampMin = "0.05"))
	float AttackInterval = 1.5f;

	/** Radius (cm) within which the soldier looks for a hostile target to engage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Combat", meta = (ClampMin = "0.0"))
	float DetectionRadius = 1500.f;

	/**
	 * Think-loop period (s). The soldier re-evaluates target/movement/attack on a timer
	 * instead of Actor Tick (ARCHITECTURE.md 18.3) and serves as the per-unit AI update
	 * interval (9.3). 0.2 = 5 Hz, appropriate for a near-range Actor soldier.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|AI", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.2f;

	/** Acceptance radius (cm) passed to the move request so soldiers don't jitter onto a goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Movement", meta = (ClampMin = "0.0"))
	float MoveAcceptanceRadius = 80.f;

	/** Seconds the corpse persists after death before being destroyed (0 = keep). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Health", meta = (ClampMin = "0.0"))
	float DeathLifeSpan = 3.f;
};
