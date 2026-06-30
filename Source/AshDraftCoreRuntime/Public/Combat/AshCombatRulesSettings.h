// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Mass/EntityHandle.h"
#include "AshCombatRulesSettings.generated.h"

struct FMassEntityManager;

/**
 * UAshCombatRulesSettings
 *
 * Game-wide combat rule data (ARCHITECTURE.md 17: tunable values live in data, not hardcoded). A
 * UDeveloperSettings so the rules are globally reachable via GetDefault<UAshCombatRulesSettings>() without
 * passing references around, and editable under Project Settings -> Game -> "Ash Combat Rules" (persisted
 * to DefaultGame.ini). This is the home for cross-cutting combat rules that are not tied to one character
 * or unit type; today it holds the stun source-immunity window, and more rules can be added here later.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Ash Combat Rules"))
class ASHDRAFTCORERUNTIME_API UAshCombatRulesSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	/**
	 * Seconds a victim is immune to being stunned by a *new* attacker after its last stun (Phase 32).
	 * Consecutive stuns from the SAME attacker (a continuous combo) are always allowed — they are
	 * unavoidable by design — but once stunned, a *different* attacker cannot stun the victim again until
	 * this window elapses. This guarantees a counterattack opportunity while the first attacker is on its
	 * cooldown and prevents infinite stun-locking by a crowd. Knockback is unaffected (it never blocks
	 * input). 0 disables the rule (any attacker may stun at any time).
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Stun", meta = (ClampMin = "0.0"))
	float NewStunSourceImmunity = 2.0f;
};

/**
 * Shared combat-rule helpers operating on Mass soldier entities (kept out of the settings UObject so the
 * processors and the proxy actor share one implementation of the hit-reaction rule).
 */
namespace AshCombat
{
	/**
	 * Applies hit-reaction stun + slight knockback to a Mass soldier victim, honoring the game-wide
	 * new-source stun immunity (UAshCombatRulesSettings::NewStunSourceImmunity). SourceId identifies the
	 * attacker (the attacker's Mass entity index, or an actor's UniqueID) so a continuing combo from the
	 * same source re-stuns freely while a *different* source is locked out for the immunity window. Reads
	 * the victim's UAshSoldierBehaviorConfig for the stun length / knockback speed; no-op if the victim has
	 * no stun fragment or its unit type has StunDuration 0. Now is the current world time (seconds).
	 */
	ASHDRAFTCORERUNTIME_API void ApplySoldierStun(FMassEntityManager& EntityManager, const FMassEntityHandle& Victim,
		const FVector& FromLocation, int32 SourceId, float Now);
}
