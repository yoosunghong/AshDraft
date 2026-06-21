// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "AshCombatSlotTestActor.generated.h"

class UAshCombatSlotComponent;

/**
 * AAshCombatSlotTestActor
 *
 * Phase 6 verification helper for UAshCombatSlotComponent (mirrors the
 * AAshGASTargetDummy dev-helper pattern). Place one in the PoC map: on BeginPlay it
 * spawns a ring of lightweight dummy "attackers" (plain AActors) around itself at
 * random offsets and has each request a slot, then logs / on-screen-prints which slot
 * each dummy received and where that slot is in the world.
 *
 * It also enables the component's debug ring so slot occupancy is visible in PIE
 * (green = empty, yellow = reserved, orange = occupied). Optionally it releases a
 * subset of slots after a delay to show reservation churn (FindBestAvailableSlot
 * re-filling freed slots).
 *
 * Dev/verification only — not shipping gameplay.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshCombatSlotTestActor : public AActor
{
	GENERATED_BODY()

public:
	AAshCombatSlotTestActor();

	virtual void BeginPlay() override;

protected:
	/** Spawns NumTestRequesters dummies, requests a slot for each, and logs the result. */
	void RunSlotReservationTest();

	/** Releases the slots held by the first NumToRelease dummies and re-runs requests for the rest. */
	void ReleaseSubsetAndRetest();

	/** The component under test. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAshCombatSlotComponent> CombatSlots;

	/** How many dummy attackers to spawn and try to seat. Set above slot count to exercise the "no slot free" path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 NumTestRequesters = 10;

	/** Distance (cm) from center at which dummies are spawned before requesting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DummySpawnRadius = 400.f;

	/** After this delay (s) release a subset to show churn. Zero disables the second pass. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float ReleaseTestDelay = 3.f;

private:
	/** Spawned dummy requesters (plain actors used only as slot occupants). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Dummies;
};
