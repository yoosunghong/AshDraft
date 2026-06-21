// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshCombatSlotConfig.generated.h"

/**
 * UAshCombatSlotConfig
 *
 * Data-driven tuning for a UAshCombatSlotComponent (ARCHITECTURE.md 10 / 17). Slot
 * count, ring radius, and release behavior must not be hardcoded on the component;
 * a target (hero, enemy general, base) references one of these assets so attackers
 * can be re-tuned without recompiling.
 *
 * The component falls back to its own inline defaults when no config is assigned, so
 * a target is still functional before an author creates a DA_CombatSlots_* asset.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshCombatSlotConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Number of evenly-spaced directional slots around the target (8 = the design default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot", meta = (ClampMin = "1", ClampMax = "32"))
	int32 NumSlots = 8;

	/** Distance (cm) from the target center to each slot — the surround ring radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot", meta = (ClampMin = "0.0"))
	float SlotRadius = 175.f;

	/**
	 * Yaw offset (degrees) applied to slot 0. Slot 0 defaults to local +X (forward);
	 * use this to rotate the whole ring if a target's forward should not own a slot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot")
	float StartAngleOffsetDegrees = 0.f;
};
