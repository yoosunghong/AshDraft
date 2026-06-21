// Copyright YoosungHong. All Rights Reserved.

#include "Combat/AshCombatSlotComponent.h"

#include "Combat/AshCombatSlotConfig.h"
#include "GameFramework/Actor.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

UAshCombatSlotComponent::UAshCombatSlotComponent()
{
	// Tick is off by default; only enabled (in BeginPlay) when debug drawing is on.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// We want the ring populated before BeginPlay so early RequestSlot calls work.
	bWantsInitializeComponent = true;
}

void UAshCombatSlotComponent::InitializeComponent()
{
	Super::InitializeComponent();
	RebuildSlots();
}

void UAshCombatSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	// Components added in the editor may skip InitializeComponent; guarantee a valid ring.
	if (Slots.Num() != ResolveNumSlots())
	{
		RebuildSlots();
	}

	SetComponentTickEnabled(bDrawDebugSlots);
}

void UAshCombatSlotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Tick exists solely for debug visualization (ARCHITECTURE.md 18.3).
	if (bDrawDebugSlots)
	{
		DrawDebugSlots();
	}
}

void UAshCombatSlotComponent::RebuildSlots()
{
	Slots.Reset();
	Slots.SetNum(ResolveNumSlots());
}

int32 UAshCombatSlotComponent::ResolveNumSlots() const
{
	return Config ? Config->NumSlots : FallbackNumSlots;
}

float UAshCombatSlotComponent::ResolveSlotRadius() const
{
	return Config ? Config->SlotRadius : FallbackSlotRadius;
}

float UAshCombatSlotComponent::ResolveStartAngleOffset() const
{
	return Config ? Config->StartAngleOffsetDegrees : FallbackStartAngleOffset;
}

int32 UAshCombatSlotComponent::RequestSlot(AActor* Requester)
{
	if (!IsValid(Requester) || Slots.Num() == 0)
	{
		return INDEX_NONE;
	}

	// Idempotent: an actor that already holds a slot keeps it.
	const int32 ExistingSlot = GetSlotForOccupant(Requester);
	if (ExistingSlot != INDEX_NONE)
	{
		return ExistingSlot;
	}

	const int32 BestSlot = FindBestAvailableSlot(Requester->GetActorLocation());
	if (BestSlot == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	Slots[BestSlot].State = EAshCombatSlotState::Reserved;
	Slots[BestSlot].Occupant = Requester;
	return BestSlot;
}

bool UAshCombatSlotComponent::ReleaseSlot(AActor* Requester)
{
	const int32 SlotIndex = GetSlotForOccupant(Requester);
	return ReleaseSlotByIndex(SlotIndex);
}

bool UAshCombatSlotComponent::ReleaseSlotByIndex(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	const bool bWasHeld = Slots[SlotIndex].State != EAshCombatSlotState::Empty || Slots[SlotIndex].Occupant.IsValid();
	Slots[SlotIndex].State = EAshCombatSlotState::Empty;
	Slots[SlotIndex].Occupant = nullptr;
	return bWasHeld;
}

FVector UAshCombatSlotComponent::GetSlotWorldLocation(int32 SlotIndex) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Slots.IsValidIndex(SlotIndex) || Slots.Num() == 0)
	{
		return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	}

	// Even angular split, slot 0 at the owner's forward (+X) plus the configured offset.
	const float SliceDegrees = 360.f / static_cast<float>(Slots.Num());
	const float LocalYaw = ResolveStartAngleOffset() + SliceDegrees * static_cast<float>(SlotIndex);

	// Rotate the local ring direction by the owner's yaw so the ring follows the target's facing.
	const FRotator OwnerYaw(0.f, Owner->GetActorRotation().Yaw, 0.f);
	const FVector LocalDir = FRotator(0.f, LocalYaw, 0.f).Vector();
	const FVector WorldDir = OwnerYaw.RotateVector(LocalDir);

	return Owner->GetActorLocation() + WorldDir * ResolveSlotRadius();
}

int32 UAshCombatSlotComponent::FindBestAvailableSlot(const FVector& FromLocation) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].State != EAshCombatSlotState::Empty)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(FromLocation, GetSlotWorldLocation(Index));
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = Index;
		}
	}

	return BestIndex;
}

int32 UAshCombatSlotComponent::GetSlotForOccupant(const AActor* Requester) const
{
	if (!Requester)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].Occupant.Get() == Requester)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UAshCombatSlotComponent::SetSlotState(AActor* Requester, EAshCombatSlotState NewState)
{
	const int32 SlotIndex = GetSlotForOccupant(Requester);
	if (SlotIndex == INDEX_NONE)
	{
		return false;
	}

	// Releasing should go through ReleaseSlot so the occupant is cleared too.
	if (NewState == EAshCombatSlotState::Empty)
	{
		return ReleaseSlotByIndex(SlotIndex);
	}

	Slots[SlotIndex].State = NewState;
	return true;
}

EAshCombatSlotState UAshCombatSlotComponent::GetSlotState(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].State : EAshCombatSlotState::Empty;
}

void UAshCombatSlotComponent::DrawDebugSlots() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const FVector Center = Owner->GetActorLocation();
	DrawDebugCircle(World, Center, ResolveSlotRadius(), 32, FColor(80, 80, 80), false, -1.f, 0,
		2.f, FVector(0, 1, 0), FVector(1, 0, 0), false);

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		FColor SlotColor;
		switch (Slots[Index].State)
		{
		case EAshCombatSlotState::Empty:     SlotColor = FColor::Green;  break;
		case EAshCombatSlotState::Reserved:  SlotColor = FColor::Yellow; break;
		case EAshCombatSlotState::Occupied:  SlotColor = FColor::Orange; break;
		case EAshCombatSlotState::Attacking: SlotColor = FColor::Red;    break;
		case EAshCombatSlotState::Cooldown:  SlotColor = FColor::Blue;   break;
		default:                             SlotColor = FColor::White;  break;
		}

		const FVector SlotLoc = GetSlotWorldLocation(Index);
		DrawDebugSphere(World, SlotLoc, 24.f, 8, SlotColor, false, -1.f, 0, 1.5f);
		DrawDebugLine(World, Center, SlotLoc, FColor(40, 40, 40), false, -1.f, 0, 1.f);
		DrawDebugString(World, SlotLoc + FVector(0, 0, 30.f), FString::FromInt(Index), nullptr, SlotColor, 0.f);
	}
#endif // ENABLE_DRAW_DEBUG
}
