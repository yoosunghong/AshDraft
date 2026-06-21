// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "AshCombatSlotComponent.generated.h"

class UAshCombatSlotConfig;

/**
 * State of a single combat slot (ARCHITECTURE.md 10.2).
 *
 * PoC scope only models the reservation lifecycle the slot manager owns. Attacking /
 * Cooldown are exposed so an attacker's AI/GAS can stamp its current intent onto the
 * slot it already holds (the component never drives combat itself, 10.3).
 */
UENUM(BlueprintType)
enum class EAshCombatSlotState : uint8
{
	/** Free to be requested. */
	Empty		UMETA(DisplayName = "Empty"),

	/** Claimed by an occupant that is still moving into position. */
	Reserved	UMETA(DisplayName = "Reserved"),

	/** Occupant has arrived and is holding the slot. */
	Occupied	UMETA(DisplayName = "Occupied"),

	/** Occupant is mid-attack (set by the attacker, not the component). */
	Attacking	UMETA(DisplayName = "Attacking"),

	/** Occupant is recovering after an attack (set by the attacker). */
	Cooldown	UMETA(DisplayName = "Cooldown")
};

/**
 * One directional attack position around the target.
 */
USTRUCT(BlueprintType)
struct FAshCombatSlot
{
	GENERATED_BODY()

	/** Current lifecycle state. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|CombatSlot")
	EAshCombatSlotState State = EAshCombatSlotState::Empty;

	/** The actor currently holding (Reserved/Occupied/...) this slot, if any. Weak: never keeps an attacker alive. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|CombatSlot")
	TWeakObjectPtr<AActor> Occupant;
};

/**
 * UAshCombatSlotComponent
 *
 * Reusable 8-direction melee slot manager attached to a high-value combat target —
 * the player hero, the enemy general, or any actor that should be surrounded
 * readably (ARCHITECTURE.md 10). It carves the ring around the owner into N evenly
 * spaced slots and hands them out one-per-attacker so soldiers spread around the
 * target instead of clumping into one overlapping mass.
 *
 * Responsibilities are deliberately narrow (10.3): reserve, release, locate, and
 * pick the best slot. It owns no AI and drives no combat — an attacker requests a
 * slot, walks to GetSlotWorldLocation, and releases it on death / disengage. Slot
 * geometry is data-driven through UAshCombatSlotConfig (17).
 *
 * Tick is disabled by default; it is only enabled while bDrawDebugSlots is true, to
 * render the debug ring (ARCHITECTURE.md 18.3 — Tick used solely for dev visualization).
 */
UCLASS(ClassGroup = (Ash), meta = (BlueprintSpawnableComponent))
class ASHDRAFTCORERUNTIME_API UAshCombatSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAshCombatSlotComponent();

	//~UActorComponent interface
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UActorComponent interface

	/**
	 * Reserve the best available slot for Requester.
	 *
	 * If Requester already holds a slot, that slot's index is returned unchanged
	 * (idempotent). The chosen slot is set to Reserved and stamped with Requester.
	 *
	 * @return the granted slot index, or INDEX_NONE if every slot is taken / Requester is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ash|CombatSlot")
	int32 RequestSlot(AActor* Requester);

	/**
	 * Release whatever slot Requester holds, returning it to Empty.
	 *
	 * @return true if a slot was held and freed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ash|CombatSlot")
	bool ReleaseSlot(AActor* Requester);

	/** Release a specific slot by index regardless of occupant (e.g. cleanup). */
	UFUNCTION(BlueprintCallable, Category = "Ash|CombatSlot")
	bool ReleaseSlotByIndex(int32 SlotIndex);

	/**
	 * World-space position of a slot, on the ring around the owner.
	 * Honors the owner's current location and yaw so the ring rotates with the target.
	 */
	UFUNCTION(BlueprintPure, Category = "Ash|CombatSlot")
	FVector GetSlotWorldLocation(int32 SlotIndex) const;

	/**
	 * Index of the empty slot whose world location is closest to FromLocation
	 * (the natural slot for an attacker approaching from that side).
	 *
	 * @return best empty slot index, or INDEX_NONE if none are free.
	 */
	UFUNCTION(BlueprintPure, Category = "Ash|CombatSlot")
	int32 FindBestAvailableSlot(const FVector& FromLocation) const;

	/** Index of the slot Requester currently holds, or INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "Ash|CombatSlot")
	int32 GetSlotForOccupant(const AActor* Requester) const;

	/** Mark an already-held slot with a combat sub-state (Occupied/Attacking/Cooldown). No-op if the actor holds no slot. */
	UFUNCTION(BlueprintCallable, Category = "Ash|CombatSlot")
	bool SetSlotState(AActor* Requester, EAshCombatSlotState NewState);

	/** Read-only slot state by index. */
	UFUNCTION(BlueprintPure, Category = "Ash|CombatSlot")
	EAshCombatSlotState GetSlotState(int32 SlotIndex) const;

	/** Number of slots in the ring (from config, or the inline default). */
	UFUNCTION(BlueprintPure, Category = "Ash|CombatSlot")
	int32 GetNumSlots() const { return Slots.Num(); }

	void SetDrawDebugSlots(bool bDraw) { bDrawDebugSlots = bDraw; SetComponentTickEnabled(bDraw); }

protected:
	/** (Re)builds the Slots array to NumSlots empty slots. Safe to call before/after BeginPlay. */
	void RebuildSlots();

	/** Effective slot count — config if set, else the inline fallback. */
	int32 ResolveNumSlots() const;

	/** Effective ring radius — config if set, else the inline fallback. */
	float ResolveSlotRadius() const;

	/** Effective slot-0 yaw offset — config if set, else the inline fallback. */
	float ResolveStartAngleOffset() const;

	/** Draws the debug ring (slot positions + occupancy color) for one frame. */
	void DrawDebugSlots() const;

	/** Data-driven slot geometry. When null, the inline Fallback* values below are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot")
	TObjectPtr<UAshCombatSlotConfig> Config;

	/** Inline fallback slot count when Config is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot|Fallback", meta = (ClampMin = "1", ClampMax = "32"))
	int32 FallbackNumSlots = 8;

	/** Inline fallback ring radius (cm) when Config is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot|Fallback", meta = (ClampMin = "0.0"))
	float FallbackSlotRadius = 175.f;

	/** Inline fallback slot-0 yaw offset (deg) when Config is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|CombatSlot|Fallback")
	float FallbackStartAngleOffset = 0.f;

	/** When true, render the slot ring every frame (enables Tick). Dev-only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|CombatSlot|Debug")
	bool bDrawDebugSlots = false;

private:
	/** Runtime slot ring. Index == direction index; angle is derived from the index. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Ash|CombatSlot")
	TArray<FAshCombatSlot> Slots;
};
