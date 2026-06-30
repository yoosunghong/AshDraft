// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Math/Vector.h"
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

	/**
	 * Human-readable name for UI (the HUD's recently-struck-enemy panel, Phase 30). Optional: the
	 * default is empty, and callers fall back to a generic team-based label, so existing implementers
	 * need no change.
	 */
	virtual FText GetAshDisplayName() const { return FText::GetEmpty(); }

	/**
	 * Optional portrait texture for the target-info panel (Phase 30). Returns null by default;
	 * implement in hero/general characters that carry a UAshHeroConfig with a Portrait asset.
	 * Callers guard for null so implementers may return null freely.
	 */
	virtual UTexture2D* GetAshPortrait() const { return nullptr; }

	/**
	 * Universal hit reaction (Phase 32): the agent is pushed back ever so slightly and briefly stunned
	 * (Ash.State.Stunned), unable to move or attack for the stun window. SourceLocation is the attacker's
	 * world position (knock the agent back away from it); SourceId is a stable identifier of the attacker
	 * (an actor's UniqueID, or an attacker Mass entity index) used by the game-wide new-source stun-immunity
	 * rule. Default is a no-op so non-combatant implementers (e.g. a verification dummy) need no change; the
	 * hero and general override it.
	 */
	virtual void ApplyHitReaction(const FVector& SourceLocation, int32 SourceId) {}
};
