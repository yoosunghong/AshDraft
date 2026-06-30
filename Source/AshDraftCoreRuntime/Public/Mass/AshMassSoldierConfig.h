// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshMassSoldierConfig.generated.h"

class UAshSoldierVisualConfig;
class UAshSoldierBehaviorConfig;

/**
 * UAshMassSoldierConfig
 *
 * The data-driven definition of one Mass soldier *unit type* (ARCHITECTURE.md 17 / 7.4): its
 * gameplay tunables (health / movement / combat) plus a reference to the visual set used to
 * render it. It is the Mass-side counterpart to the Phase 8 UAshSoldierConfig, stripped of
 * Actor-only fields (no think interval, no nav acceptance) because a Mass soldier has no
 * per-unit Actor or timer.
 *
 * A spawner (AAshMassSoldierSpawner) points at one of these; it seeds each entity's
 * FAshHealthFragment / FAshMovementFragment / FAshCombatFragment from the stats and stamps the
 * Visual onto FAshVisualFragment, so a whole army — or a whole new unit type — is authored as
 * data without recompiling. The spawner falls back to inline defaults when no config is assigned.
 *
 * Adding a unit type (infantry / archer / cavalry) = a new asset of this class with its own stats
 * and a Visual reference; no Blueprint or code changes.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshMassSoldierConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Visual + animation set for this unit type. Drives the representation proxy's mesh/anim. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Visual")
	TObjectPtr<UAshSoldierVisualConfig> Visual;

	/**
	 * Local-AI behavior set for this unit type (Phase 20): sensing radius, leash, facing turn rate,
	 * separation tuning, ground conform. Drives the behavior / movement / ground processors. Null is
	 * tolerated — the processors fall back to built-in defaults.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Behavior")
	TObjectPtr<UAshSoldierBehaviorConfig> Behavior;

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

	/**
	 * Seconds between attack *cycles* (one cycle = a 1-3 hit combo). Default 3 s (Phase 29): every soldier
	 * waits this long after finishing a combo before it may start another, and releases its attack slot for
	 * that window. Seeds FAshCombatFragment::AttackCooldown.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Combat", meta = (ClampMin = "0.05"))
	float AttackCooldown = 3.0f;

	/**
	 * Per-cycle randomization of the attack cooldown (0..1). Each cycle's interval is drawn from
	 * AttackCooldown * [1 - variance, 1 + variance] so soldiers trade blows on staggered timing instead
	 * of the whole line swinging in unison. Seeds FAshCombatFragment::AttackCooldownVariance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackCooldownVariance = 0.4f;

	/**
	 * Seconds between the consecutive hits of one combo (Phase 29). Paces the burst of a 2- or 3-hit combo;
	 * tune to match the unit's combo attack montages. Seeds FAshCombatFragment::ComboHitInterval.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|MassSoldier|Combat", meta = (ClampMin = "0.05"))
	float ComboHitInterval = 0.45f;
};
