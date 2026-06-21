// Copyright YoosungHong. All Rights Reserved.

#include "Character/AshEnemyGeneralCharacter.h"

#include "AbilitySystem/AshAbilitySystemComponent.h"
#include "AbilitySystem/AshAttributeSet.h"
#include "AbilitySystem/AshGameplayAbility.h"
#include "AI/AshEnemyGeneralController.h"
#include "AshGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AAshEnemyGeneralCharacter::AAshEnemyGeneralCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No per-frame Actor Tick (ARCHITECTURE.md 18.3); behavior is BT/GAS/delegate-driven.
	PrimaryActorTick.bCanEverTick = false;

	// AI possesses the general (placed or spawned); the controller runs the Behavior Tree.
	AIControllerClass = AAshEnemyGeneralController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Face travel direction; movement is NavMesh-driven by the AI (ARCHITECTURE.md 12).
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, 480.f, 0.f);
		MoveComp->bConstrainToPlane = true;
		MoveComp->bSnapToPlaneAtStart = true;
		MoveComp->MaxWalkSpeed = 450.f;
	}

	// GAS: single-player PoC keeps the ASC on the avatar (ARCHITECTURE.md 5), replicated
	// so damage/death stay applicable from an authoritative context (5.5 / 15).
	AbilitySystemComponent = CreateDefaultSubobject<UAshAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAshAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AAshEnemyGeneralCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAshEnemyGeneralCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Authority side: bind the ASC to this avatar, seed attributes, grant abilities.
	InitializeAbilitySystem();
}

void AAshEnemyGeneralCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		// Watch Health so we can react to hits and trigger death at zero.
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAshAttributeSet::GetHealthAttribute()).AddUObject(this, &AAshEnemyGeneralCharacter::OnHealthChanged);
	}
}

void AAshEnemyGeneralCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAshAttributeSet::GetHealthAttribute()).RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AAshEnemyGeneralCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		InitializeAttributes();
		GrantDefaultAbilities();
	}
}

void AAshEnemyGeneralCharacter::InitializeAttributes()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	// Seed base values from the editor-tunable initial stats (ARCHITECTURE.md 17).
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxHealthAttribute(), InitialMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetHealthAttribute(), InitialMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetAttackPowerAttribute(), InitialAttackPower);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetDefenseAttribute(), InitialDefense);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxStaminaAttribute(), InitialMaxStamina);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetStaminaAttribute(), InitialMaxStamina);
}

void AAshEnemyGeneralCharacter::GrantDefaultAbilities()
{
	if (!AbilitySystemComponent || bAbilitiesGranted)
	{
		return;
	}

	for (const TSubclassOf<UAshGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);

		// Stamp the ability's input tag so the AI can trigger it via AbilityInputTagPressed.
		if (const UAshGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject())
		{
			const FGameplayTag& AbilityInputTag = AbilityCDO->GetInputTag();
			if (AbilityInputTag.IsValid())
			{
				Spec.GetDynamicSpecSourceTags().AddTag(AbilityInputTag);
			}
		}

		AbilitySystemComponent->GiveAbility(Spec);
	}

	bAbilitiesGranted = true;
}

void AAshEnemyGeneralCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bIsDead)
	{
		return;
	}

	// Death is event-driven (ARCHITECTURE.md 15): the attribute set has already applied
	// Ash.State.Dead at zero; here we handle the actor-side consequences once.
	if (Data.NewValue <= 0.f)
	{
		HandleDeath();
	}
}

void AAshEnemyGeneralCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// Stop the brain so the dead general no longer chases or attacks.
	if (AAshEnemyGeneralController* AIController = Cast<AAshEnemyGeneralController>(GetController()))
	{
		AIController->StopBehavior();
	}

	// Cancel any in-flight abilities and tag dead (attribute set also tags on the ASC).
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(AshGameplayTags::State_Dead);
	}

	// Disable further interaction and stop movement (placeholder for a death montage/ragdoll).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	OnGeneralDied.Broadcast(this);

	if (DeathLifeSpan > 0.f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
}

float AAshEnemyGeneralCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AAshEnemyGeneralCharacter::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}
