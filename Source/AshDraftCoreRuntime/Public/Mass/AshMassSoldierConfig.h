// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshMassSoldierConfig.generated.h"

/**
 * UAshMassSoldierConfig
 *
 * Data-driven archetype tunables for Mass Entity soldiers (ARCHITECTURE.md 17 / 7.4).
 * This is the Mass-side counterpart to the Phase 8 UAshSoldierConfig: the same kind of
 * health / movement / combat numbers, but stripped of Actor-only fields (no think
 * interval, no nav acceptance) because a Mass soldier has no per-unit Actor or timer.
 *
 * The spawner (AAshMassSoldierSpawner) seeds each entity's FAshHealthFragment /
 * FAshMovementFragment / FAshCombatFragment from these values, so a whole army can be
 * tuned without recompiling. The spawner falls back to inline defaults when no config
 * is assigned.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshMassSoldierConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Full (and initial) health. Seeds FAshHealthFragment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 50.f;

	/** Ground movement speed (cm/s). Seeds FAshMovementFragment::MoveSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeed = 350.f;

	/** Distance (cm) within which the soldier may strike. Seeds FAshCombatFragment::AttackRange. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 150.f;

	/** Damage applied per successful attack. Seeds FAshCombatFragment::AttackPower. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Combat", meta = (ClampMin = "0.0"))
	float AttackPower = 10.f;

	/** Seconds between attacks. Seeds FAshCombatFragment::AttackCooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Combat", meta = (ClampMin = "0.05"))
	float AttackCooldown = 1.5f;
};
