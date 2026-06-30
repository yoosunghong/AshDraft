// Copyright YoosungHong. All Rights Reserved.

#include "UI/AshCombatFeedSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UAshCombatFeedSubsystem* UAshCombatFeedSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UAshCombatFeedSubsystem>() : nullptr;
}

bool UAshCombatFeedSubsystem::IsLocalPlayerPawn(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC && PC->GetPawn() == Actor;
}

void UAshCombatFeedSubsystem::ReportPlayerStrike(const AActor* Instigator, const FText& UnitName, EAshTeamId Team, float HealthFraction, bool bKilled, UTexture2D* Portrait)
{
	// Only the local player's own strikes drive the HUD (AI-vs-AI damage is telemetry's concern).
	if (!IsLocalPlayerPawn(Instigator))
	{
		return;
	}

	LastStruckUnit.UnitName = UnitName;
	LastStruckUnit.Team = Team;
	LastStruckUnit.HealthFraction = FMath::Clamp(HealthFraction, 0.f, 1.f);
	LastStruckUnit.Portrait = Portrait;
	LastStruckUnit.bValid = true;
	OnUnitStruck.Broadcast(LastStruckUnit);

	if (bKilled)
	{
		++PlayerKillCount;
		OnPlayerKillCountChanged.Broadcast(PlayerKillCount);
	}
}

void UAshCombatFeedSubsystem::ResetFeed()
{
	PlayerKillCount = 0;
	LastStruckUnit = FAshStruckUnitInfo();
	OnPlayerKillCountChanged.Broadcast(PlayerKillCount);
}
