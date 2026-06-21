// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Teams/AshTeamTypes.h"
#include "AshTeamAgentInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAshTeamAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * IAshTeamAgentInterface
 *
 * Minimal team-identity contract shared by AshDraft combatants (hero, enemy general,
 * verification dummy, future soldiers). It lets team-aware systems — combat slots,
 * bases, AI targeting — ask any actor for its EAshTeamId without depending on a
 * concrete class (ARCHITECTURE.md 18.4: prefer IDs/handles over hard references).
 *
 * Resolve an arbitrary actor's team through UAshTeamStatics::GetActorTeam, which also
 * checks the actor's controller, rather than casting to this interface by hand.
 */
class IAshTeamAgentInterface
{
	GENERATED_BODY()

public:
	/** This agent's battlefield team. */
	virtual EAshTeamId GetAshTeamId() const = 0;
};
