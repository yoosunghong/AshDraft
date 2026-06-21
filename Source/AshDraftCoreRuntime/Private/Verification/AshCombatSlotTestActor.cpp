// Copyright YoosungHong. All Rights Reserved.

#include "Verification/AshCombatSlotTestActor.h"

#include "Combat/AshCombatSlotComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshCombatSlotTest, Log, All);

namespace
{
	void ReportLine(const FString& Message, const FColor& Color)
	{
		UE_LOG(LogAshCombatSlotTest, Log, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, Color, Message);
		}
	}
}

AAshCombatSlotTestActor::AAshCombatSlotTestActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CombatSlots = CreateDefaultSubobject<UAshCombatSlotComponent>(TEXT("CombatSlots"));
}

void AAshCombatSlotTestActor::BeginPlay()
{
	Super::BeginPlay();

	// Make the slot ring visible for the duration of the test.
	if (CombatSlots)
	{
		CombatSlots->SetDrawDebugSlots(true);
		CombatSlots->SetComponentTickEnabled(true);
	}

	RunSlotReservationTest();

	if (ReleaseTestDelay > 0.f)
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &AAshCombatSlotTestActor::ReleaseSubsetAndRetest, ReleaseTestDelay, false);
	}
}

void AAshCombatSlotTestActor::RunSlotReservationTest()
{
	if (!CombatSlots)
	{
		return;
	}

	const int32 NumSlots = CombatSlots->GetNumSlots();
	ReportLine(FString::Printf(TEXT("[CombatSlot] Test start: %d requesters for %d slots"), NumTestRequesters, NumSlots), FColor::Cyan);

	const FVector Center = GetActorLocation();
	int32 GrantedCount = 0;

	for (int32 i = 0; i < NumTestRequesters; ++i)
	{
		// Spread dummies around the ring at random-ish angles so FindBestAvailableSlot has variety.
		const float Angle = FMath::FRand() * 360.f;
		const FVector Offset = FRotator(0.f, Angle, 0.f).Vector() * DummySpawnRadius;
		const FVector SpawnLoc = Center + Offset;

		AActor* Dummy = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), SpawnLoc, FRotator::ZeroRotator);
		if (!Dummy)
		{
			continue;
		}
		Dummies.Add(Dummy);

		const int32 SlotIndex = CombatSlots->RequestSlot(Dummy);
		if (SlotIndex != INDEX_NONE)
		{
			++GrantedCount;
			CombatSlots->SetSlotState(Dummy, EAshCombatSlotState::Occupied);
			const FVector SlotLoc = CombatSlots->GetSlotWorldLocation(SlotIndex);
			ReportLine(FString::Printf(TEXT("  Dummy %d -> slot %d @ (%.0f, %.0f, %.0f)"), i, SlotIndex, SlotLoc.X, SlotLoc.Y, SlotLoc.Z), FColor::Green);
		}
		else
		{
			ReportLine(FString::Printf(TEXT("  Dummy %d -> NO SLOT (ring full)"), i), FColor::Red);
		}
	}

	ReportLine(FString::Printf(TEXT("[CombatSlot] Granted %d / %d (expected min(requesters, slots) = %d)"),
		GrantedCount, NumTestRequesters, FMath::Min(NumTestRequesters, NumSlots)), FColor::Cyan);
}

void AAshCombatSlotTestActor::ReleaseSubsetAndRetest()
{
	if (!CombatSlots)
	{
		return;
	}

	const int32 NumToRelease = FMath::Max(1, Dummies.Num() / 2);
	int32 Released = 0;
	for (int32 i = 0; i < NumToRelease && i < Dummies.Num(); ++i)
	{
		if (Dummies[i] && CombatSlots->ReleaseSlot(Dummies[i]))
		{
			++Released;
		}
	}
	ReportLine(FString::Printf(TEXT("[CombatSlot] Released %d slots; re-requesting for previously-denied dummies"), Released), FColor::Yellow);

	// Any dummy that has no slot now tries again — freed slots should be re-granted.
	int32 ReGranted = 0;
	for (AActor* Dummy : Dummies)
	{
		if (Dummy && CombatSlots->GetSlotForOccupant(Dummy) == INDEX_NONE)
		{
			const int32 SlotIndex = CombatSlots->RequestSlot(Dummy);
			if (SlotIndex != INDEX_NONE)
			{
				++ReGranted;
				CombatSlots->SetSlotState(Dummy, EAshCombatSlotState::Occupied);
			}
		}
	}
	ReportLine(FString::Printf(TEXT("[CombatSlot] Re-granted %d freed slots (expected ~%d)"), ReGranted, Released), FColor::Yellow);
}
