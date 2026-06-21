// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshBaseConfig.generated.h"

/**
 * UAshBaseConfig
 *
 * Data-driven tuning for AAshBaseActor (ARCHITECTURE.md 11 / 17). Durability, capture
 * radius, drain/recovery rates, and timings must not be hardcoded on the base actor;
 * a placed base references one of these so the durability-capture model can be tuned
 * without recompiling. The base falls back to inline defaults when none is assigned.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshBaseConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Full durability (and the value restored on capture). Defender losses drain toward zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (ClampMin = "1.0"))
	float MaxDurability = 100.f;

	/** Radius (cm) of the capture volume; units inside it count as defenders/attackers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (ClampMin = "1.0"))
	float CaptureRadius = 600.f;

	/** Durability lost per second while attackers hold the base uncontested (presence-driven PoC drain). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (ClampMin = "0.0"))
	float CaptureRatePerSecond = 25.f;

	/** Durability restored per second once the base is uncontested and past the recovery delay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (ClampMin = "0.0"))
	float RecoveryRatePerSecond = 15.f;

	/** Quiet time (s) with no attackers before durability begins recovering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (ClampMin = "0.0"))
	float RecoveryDelay = 3.f;

	/** Durability subtracted per defender death (Phase 8 soldiers call AAshBaseActor::NotifyDefenderDied). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (ClampMin = "0.0"))
	float DurabilityPerDefenderDeath = 10.f;

	/** Capture-state update period (s). Timer-driven instead of Actor Tick (ARCHITECTURE.md 18.3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (ClampMin = "0.05"))
	float UpdateInterval = 0.5f;

	/** Delay (s) before new defenders spawn after ownership flips. Placeholder until Phase 8. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (ClampMin = "0.0"))
	float DefenderRespawnDelay = 5.f;
};
