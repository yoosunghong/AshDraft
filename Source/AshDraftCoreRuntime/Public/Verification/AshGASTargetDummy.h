// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "Teams/AshTeamTypes.h"
#include "Teams/AshTeamAgentInterface.h"
#include "AshGASTargetDummy.generated.h"

class UAshAbilitySystemComponent;
class UAshAttributeSet;
class UCapsuleComponent;
struct FOnAttributeChangeData;

/**
 * AAshGASTargetDummy
 *
 * A minimal GAS-enabled training/verification target (Lyra B_TargetDummy style).
 * It exists to satisfy the Phase 4 task "verify that a target can receive damage"
 * without prematurely building the Phase 5 enemy general: it owns an
 * UAshAbilitySystemComponent + UAshAttributeSet and a Pawn-channel capsule so the
 * hero's melee sweep (UAshGA_BasicAttack, which traces ECC_Pawn and applies the
 * damage GE to any hit actor with an ability system) finds and damages it.
 *
 * On every Health change it logs and prints an on-screen message, so the damage
 * pipeline is observable in PIE. When Health reaches zero it tags itself
 * Ash.State.Dead (via the attribute set) and stops reporting.
 *
 * This is a verification/dev helper, not the real enemy general. Place an instance
 * (or a Blueprint child with a visible mesh) in the PoC map in front of the hero.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshGASTargetDummy : public AActor, public IAbilitySystemInterface, public IAshTeamAgentInterface
{
	GENERATED_BODY()

public:
	AAshGASTargetDummy();

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	//~IAshTeamAgentInterface
	virtual EAshTeamId GetAshTeamId() const override { return TeamId; }
	//~End of IAshTeamAgentInterface

	/** Current health, read from the GAS AttributeSet. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetHealth() const;

	/** Maximum health, read from the GAS AttributeSet. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetMaxHealth() const;

protected:
	/** Seeds attribute base values from the editor-tunable initial stats. */
	void InitializeAttributes();

	/** Health-changed handler: logs + prints the new value so PIE shows the damage. */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

private:
	/** Pawn-channel collision body so the hero's ECC_Pawn melee sweep hits this dummy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Collision", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CollisionCapsule;

	/** GAS ability system component owned by this dummy (PoC: ASC on the avatar). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAshAbilitySystemComponent> AbilitySystemComponent;

	/** Combat attribute set (Health/AttackPower/Defense/Stamina). */
	UPROPERTY()
	TObjectPtr<UAshAttributeSet> AttributeSet;

	/** Team identity. Defaults to Enemy so it reads as a valid hostile target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Team", meta = (AllowPrivateAccess = "true"))
	EAshTeamId TeamId = EAshTeamId::Enemy;

	/** Initial (and maximum) health seeded into the attribute set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxHealth = 100.f;

	/** Initial defense subtracted from incoming damage by the execution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InitialDefense = 0.f;
};
