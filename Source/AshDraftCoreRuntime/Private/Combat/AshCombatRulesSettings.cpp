// Copyright YoosungHong. All Rights Reserved.

#include "Combat/AshCombatRulesSettings.h"

#include "Mass/AshSoldierBehaviorConfig.h"
#include "Mass/AshSoldierFragments.h"
#include "MassEntityManager.h"

void AshCombat::ApplySoldierStun(FMassEntityManager& EntityManager, const FMassEntityHandle& Victim,
	const FVector& FromLocation, int32 SourceId, float Now)
{
	FAshStunFragment* Stun = EntityManager.GetFragmentDataPtr<FAshStunFragment>(Victim);
	if (!Stun)
	{
		return;
	}

	// Game-wide new-source immunity: a *different* attacker cannot re-stun until the window elapses; the
	// same attacker (a continuing combo) always may. This is what guarantees the counterattack window.
	const float Immunity = GetDefault<UAshCombatRulesSettings>()->NewStunSourceImmunity;
	const bool bSameSource = (SourceId != INDEX_NONE) && (SourceId == Stun->LastStunSourceId);
	if (!bSameSource && Immunity > 0.f && (Now - Stun->LastStunTime) < Immunity)
	{
		return; // still immune to a new stun source
	}

	float StunDuration = 0.35f;
	float KnockbackSpeed = 250.f;
	if (const FAshBehaviorFragment* Behavior = EntityManager.GetFragmentDataPtr<FAshBehaviorFragment>(Victim))
	{
		if (const UAshSoldierBehaviorConfig* Cfg = Behavior->Behavior)
		{
			StunDuration = Cfg->StunDuration;
			KnockbackSpeed = Cfg->KnockbackSpeed;
		}
	}

	if (StunDuration <= 0.f)
	{
		return;
	}

	FVector Dir = FVector::ZeroVector;
	if (const FAshMovementFragment* Move = EntityManager.GetFragmentDataPtr<FAshMovementFragment>(Victim))
	{
		Dir = Move->Position - FromLocation;
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal2D();
	}

	Stun->StunTimeRemaining = StunDuration; // a fresh hit refreshes the window
	Stun->StunDuration = StunDuration;
	Stun->KnockbackVelocity = Dir * KnockbackSpeed;
	Stun->LastStunSourceId = SourceId;
	Stun->LastStunTime = Now;
}
