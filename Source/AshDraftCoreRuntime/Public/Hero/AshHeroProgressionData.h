// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AshHeroProgressionData.generated.h"

/**
 * FAshHeroStatBonuses
 *
 * Flat additive stat bonuses that represent a player's upgrades to a hero archetype.
 * These are added to UAshHeroConfig base stats before seeding GAS base values, so the
 * total (base + bonuses) becomes the character's permanent attribute floor — GE modifiers
 * (buffs / debuffs) apply on top via the normal GAS stack.
 *
 * In the PoC these are edited directly on the hero Blueprint or character instance.
 * In a shipping game they would be loaded from a USaveGame and pushed onto the hero
 * before (or just after) possession so InitializeAttributes() picks them up.
 *
 * Design contract:
 *   Effective stat = UAshHeroConfig::Base* + FAshHeroStatBonuses::Bonus*
 *   AI-controlled version of the same hero uses Base* only (no bonuses applied).
 */
USTRUCT(BlueprintType)
struct FAshHeroStatBonuses
{
	GENERATED_BODY()

	/** Additional maximum health earned through upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|Hero|Progression", meta = (ClampMin = "0.0"))
	float BonusMaxHealth = 0.f;

	/** Additional attack power earned through upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|Hero|Progression", meta = (ClampMin = "0.0"))
	float BonusAttackPower = 0.f;

	/** Additional defense earned through upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|Hero|Progression", meta = (ClampMin = "0.0"))
	float BonusDefense = 0.f;

	/** Additional maximum stamina earned through upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|Hero|Progression", meta = (ClampMin = "0.0"))
	float BonusMaxStamina = 0.f;

	/** Returns true if all bonuses are zero (no progression applied). */
	bool IsEmpty() const
	{
		return BonusMaxHealth == 0.f && BonusAttackPower == 0.f
			&& BonusDefense == 0.f && BonusMaxStamina == 0.f;
	}
};
