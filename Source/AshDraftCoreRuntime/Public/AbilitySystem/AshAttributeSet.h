// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AshAttributeSet.generated.h"

/**
 * Boilerplate accessor macro for a gameplay attribute (Epic's canonical pattern).
 * Generates GetXxxAttribute(), GetXxx(), SetXxx(), and InitXxx() for a property.
 */
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UAshAttributeSet
 *
 * The initial GAS attribute set for AshDraft heroes and generals
 * (ARCHITECTURE.md 5.2). Holds the durable combat stats and a transient meta
 * attribute (Damage) used by the damage Gameplay Effect to funnel an incoming
 * hit through PostGameplayEffectExecute into Health.
 *
 * Replaces the placeholder Health/MaxHealth floats that lived on
 * AAshHeroCharacter in Phase 3.
 *
 * Authority rule (ARCHITECTURE.md 5.5): attributes are mutated through Gameplay
 * Effects so damage/death are applied from an authoritative context, keeping the
 * single-player PoC ready for future server authority.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAshAttributeSet();

	//~UAttributeSet interface
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~End of UAttributeSet interface

	/** Current health. Reaching zero applies the Ash.State.Dead tag. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UAshAttributeSet, Health);

	/** Maximum health; upper clamp for Health. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UAshAttributeSet, MaxHealth);

	/** Offensive power; the source magnitude consumed by the damage execution. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Attributes")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS(UAshAttributeSet, AttackPower);

	/** Defensive mitigation; subtracted from incoming damage by the execution. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Attributes")
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UAshAttributeSet, Defense);

	/** Current stamina; reserved for dodge / heavy-attack costs in later phases. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Attributes")
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UAshAttributeSet, Stamina);

	/** Maximum stamina; upper clamp for Stamina. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Attributes")
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UAshAttributeSet, MaxStamina);

	/**
	 * Meta attribute (not replicated, transient). The damage Gameplay Effect /
	 * execution writes the final damage here; PostGameplayEffectExecute consumes
	 * it and subtracts it from Health. Keeps Health changes authoritative and
	 * routed through the GE pipeline rather than being set directly.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|Attributes", meta = (HideFromModifiers))
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UAshAttributeSet, Damage);

private:
	/** Clamps a current/max pair on base-value changes (shared by Health/Stamina). */
	void ClampToMax(const FGameplayAttribute& Attribute, float& NewValue) const;
};
