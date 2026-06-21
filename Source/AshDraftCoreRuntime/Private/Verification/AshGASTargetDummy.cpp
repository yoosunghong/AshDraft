// Copyright YoosungHong. All Rights Reserved.

#include "Verification/AshGASTargetDummy.h"

#include "AbilitySystem/AshAbilitySystemComponent.h"
#include "AbilitySystem/AshAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "GameplayEffectExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshGASVerify, Log, All);

AAshGASTargetDummy::AAshGASTargetDummy()
{
	// Event-driven (attribute-change delegate); no per-frame Actor Tick (ARCHITECTURE.md 18.3).
	PrimaryActorTick.bCanEverTick = false;

	// Capsule on the Pawn channel: the hero's basic attack sweeps ECC_Pawn, so the
	// dummy must respond to that trace channel to be registered as a hit target.
	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	CollisionCapsule->InitCapsuleSize(42.f, 96.f);
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionCapsule->SetCollisionObjectType(ECC_Pawn);
	CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Block);
	SetRootComponent(CollisionCapsule);

	// GAS: single-player PoC keeps the ASC on the avatar (ARCHITECTURE.md 5), replicated
	// so damage stays applicable from an authoritative context (5.5 / 15).
	AbilitySystemComponent = CreateDefaultSubobject<UAshAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAshAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AAshGASTargetDummy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAshGASTargetDummy::BeginPlay()
{
	Super::BeginPlay();

	if (!AbilitySystemComponent)
	{
		return;
	}

	// Bind the ASC to this avatar so incoming gameplay effects resolve correctly.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		InitializeAttributes();
	}

	// Report Health changes so the damage path is visible in PIE (verification goal).
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UAshAttributeSet::GetHealthAttribute()).AddUObject(this, &AAshGASTargetDummy::OnHealthChanged);
}

void AAshGASTargetDummy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAshAttributeSet::GetHealthAttribute()).RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AAshGASTargetDummy::InitializeAttributes()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	// Seed base values from the editor-tunable initial stats (ARCHITECTURE.md 17).
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxHealthAttribute(), InitialMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetHealthAttribute(), InitialMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetDefenseAttribute(), InitialDefense);
}

void AAshGASTargetDummy::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	const float NewHealth = Data.NewValue;
	const float MaxHealth = GetMaxHealth();

	UE_LOG(LogAshGASVerify, Log, TEXT("[AshGASTargetDummy] %s Health: %.1f / %.1f (delta %.1f)"),
		*GetName(), NewHealth, MaxHealth, Data.NewValue - Data.OldValue);

	if (GEngine)
	{
		const FColor Color = (NewHealth <= 0.f) ? FColor::Red : FColor::Yellow;
		GEngine->AddOnScreenDebugMessage(static_cast<int32>(GetUniqueID()), 4.f, Color,
			FString::Printf(TEXT("%s took damage -> Health %.0f / %.0f"), *GetName(), NewHealth, MaxHealth));
	}
}

float AAshGASTargetDummy::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AAshGASTargetDummy::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}
