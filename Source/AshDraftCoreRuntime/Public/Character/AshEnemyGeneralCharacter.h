// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Teams/AshTeamTypes.h"
#include "Teams/AshTeamAgentInterface.h"
#include "AshEnemyGeneralCharacter.generated.h"

class UAshAbilitySystemComponent;
class UAshAttributeSet;
class UAshGameplayAbility;
struct FOnAttributeChangeData;

/** Broadcast once when the general's health first reaches zero (event-driven death). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAshOnGeneralDied, AAshEnemyGeneralCharacter*, General);

/**
 * AAshEnemyGeneralCharacter
 *
 * The high-importance enemy general (ARCHITECTURE.md 6.2). A Character-based,
 * AI-driven combatant — not a Mass Entity — because it needs high-fidelity
 * decision-making, animation, and direct combat with the player.
 *
 * It owns the same GAS foundation as the hero (UAshAbilitySystemComponent +
 * UAshAttributeSet) and is granted the shared basic-attack ability, which its
 * AI (AAshEnemyGeneralController + Behavior Tree) triggers by input tag. Death is
 * event-driven via the FAshOnGeneralDied delegate and the Ash.State.Dead tag so
 * downstream systems (AI, UI, future commander) react without a hard dependency.
 *
 * Morale is a placeholder scalar for this phase (no behavior wired yet); later
 * phases use it to drive retreat / regroup decisions.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshEnemyGeneralCharacter : public ACharacter, public IAbilitySystemInterface, public IAshTeamAgentInterface
{
	GENERATED_BODY()

public:
	AAshEnemyGeneralCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	//~APawn interface
	virtual void PossessedBy(AController* NewController) override;
	//~End of APawn interface

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	/** Team identity (Enemy by default). */
	UFUNCTION(BlueprintPure, Category = "Ash|Team")
	EAshTeamId GetTeamId() const { return TeamId; }

	//~IAshTeamAgentInterface
	virtual EAshTeamId GetAshTeamId() const override { return TeamId; }
	//~End of IAshTeamAgentInterface

	/** Current health, read from the GAS AttributeSet. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetHealth() const;

	/** Maximum health, read from the GAS AttributeSet. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetMaxHealth() const;

	/** True once health has reached zero and death has been handled. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	bool IsDead() const { return bIsDead; }

	/** Current morale placeholder (0-100). No behavior wired this phase. */
	UFUNCTION(BlueprintPure, Category = "Ash|Morale")
	float GetMorale() const { return CurrentMorale; }

	/** Reach (cm) the AI uses to decide it is close enough to attack the target. */
	UFUNCTION(BlueprintPure, Category = "Ash|Combat")
	float GetAttackRange() const { return AttackRange; }

	/** Fired when the general dies. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Health")
	FAshOnGeneralDied OnGeneralDied;

protected:
	/** Binds actor info, seeds attributes, and grants abilities (authority only). */
	void InitializeAbilitySystem();

	/** Pushes the editor-tunable initial stats into the attribute set base values. */
	void InitializeAttributes();

	/** Grants the configured default abilities, stamping each ability's input tag. */
	void GrantDefaultAbilities();

	/** Health-changed handler: drives hit reaction and triggers death at zero. */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/** One-time death handling: stops the brain, disables collision/movement, broadcasts. */
	void HandleDeath();

private:
	/** GAS ability system component owned by this general (PoC: ASC on the avatar). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAshAbilitySystemComponent> AbilitySystemComponent;

	/** Combat attribute set (Health/AttackPower/Defense/Stamina). */
	UPROPERTY()
	TObjectPtr<UAshAttributeSet> AttributeSet;

	/** Team identity. Defaults to Enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Team", meta = (AllowPrivateAccess = "true"))
	EAshTeamId TeamId = EAshTeamId::Enemy;

	/** Abilities granted on possession (authority). Each carries the input tag it binds to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UAshGameplayAbility>> DefaultAbilities;

	/** Initial (and maximum) health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxHealth = 200.f;

	/** Initial attack power fed into the damage execution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InitialAttackPower = 20.f;

	/** Initial defense subtracted from incoming damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InitialDefense = 5.f;

	/** Initial (and maximum) stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxStamina = 100.f;

	/** Morale placeholder (0-100). Tunable; unused behavior this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Morale", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "100.0"))
	float CurrentMorale = 100.f;

	/** Reach (cm) the AI uses to decide it can attack the target. Mirrors the attack ability's sweep reach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackRange = 175.f;

	/** Seconds the body persists after death before being destroyed (0 = keep). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Health", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DeathLifeSpan = 5.f;

	/** Guards one-time ability granting. */
	bool bAbilitiesGranted = false;

	/** Guards one-time death handling. */
	bool bIsDead = false;
};
