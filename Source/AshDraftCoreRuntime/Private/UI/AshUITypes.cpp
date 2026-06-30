// Copyright YoosungHong. All Rights Reserved.

#include "UI/AshUITypes.h"

#define LOCTEXT_NAMESPACE "AshUI"

FText UAshUIStatics::GetTeamDisplayText(EAshTeamId Team)
{
	switch (Team)
	{
	case EAshTeamId::Player:	return LOCTEXT("Team_Player", "Player");
	case EAshTeamId::Ally:		return LOCTEXT("Team_Ally", "Ally");
	case EAshTeamId::Enemy:		return LOCTEXT("Team_Enemy", "Enemy");
	default:					return LOCTEXT("Team_Neutral", "Neutral");
	}
}

FLinearColor UAshUIStatics::GetTeamHealthColor(EAshTeamId Team)
{
	switch (Team)
	{
	// The player's coalition (Player + Ally) reads as friendly cyan; the enemy reads as red.
	case EAshTeamId::Player:
	case EAshTeamId::Ally:
		return FLinearColor(0.f, 0.85f, 1.f, 1.f); // cyan
	case EAshTeamId::Enemy:
		return FLinearColor(1.f, 0.1f, 0.1f, 1.f); // red
	default:
		return FLinearColor(0.6f, 0.6f, 0.6f, 1.f); // neutral grey
	}
}

#undef LOCTEXT_NAMESPACE
