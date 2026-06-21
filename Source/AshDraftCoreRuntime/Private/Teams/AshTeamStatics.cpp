// Copyright YoosungHong. All Rights Reserved.

#include "Teams/AshTeamStatics.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Teams/AshTeamAgentInterface.h"

EAshTeamId UAshTeamStatics::GetActorTeam(const AActor* Actor)
{
	if (!Actor)
	{
		return EAshTeamId::Neutral;
	}

	// The actor itself (hero, general, soldier, dummy).
	if (const IAshTeamAgentInterface* TeamAgent = Cast<IAshTeamAgentInterface>(Actor))
	{
		return TeamAgent->GetAshTeamId();
	}

	// Fall back to the possessing controller for pawns whose team lives on the controller.
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const IAshTeamAgentInterface* ControllerAgent = Cast<IAshTeamAgentInterface>(Pawn->GetController()))
		{
			return ControllerAgent->GetAshTeamId();
		}
	}

	return EAshTeamId::Neutral;
}

bool UAshTeamStatics::AreTeamsHostile(EAshTeamId TeamA, EAshTeamId TeamB)
{
	// Neutral never fights.
	if (TeamA == EAshTeamId::Neutral || TeamB == EAshTeamId::Neutral)
	{
		return false;
	}

	const bool bAFriendly = (TeamA == EAshTeamId::Player || TeamA == EAshTeamId::Ally);
	const bool bBFriendly = (TeamB == EAshTeamId::Player || TeamB == EAshTeamId::Ally);

	// Player/Ally form one side; Enemy is the other. Hostile when the two are on opposite sides.
	return bAFriendly != bBFriendly;
}
