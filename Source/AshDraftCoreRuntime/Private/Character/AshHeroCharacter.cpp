// Copyright YoosungHong. All Rights Reserved.

#include "Character/AshHeroCharacter.h"

#include "AbilitySystem/AshAbilitySystemComponent.h"
#include "AbilitySystem/AshAttributeSet.h"
#include "AbilitySystem/AshGameplayAbility.h"
#include "Animation/AnimSequenceBase.h"
#include "AshGameplayTags.h"
#include "Engine/Texture2D.h"
#include "Hero/AshHeroConfig.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/AshInputConfig.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"

namespace AshHeroCharacterConstants
{
	// Default content the hero ships with. These are overridable per-Blueprint/instance.
	//static const TCHAR* SkeletalMeshPath		= TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Skins/GreatSage/Meshes/Wukong_GreatSage.Wukong_GreatSage");
	//static const TCHAR* AnimBlueprintClassPath	= TEXT("/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/AnimBP_Wukong_Rigging.AnimBP_Wukong_Rigging_C");
	//static const TCHAR* InputConfigPath			= TEXT("/AshDraftCore/Data/Input/DA_InputConfig_AshDefault.DA_InputConfig_AshDefault");
	//static const TCHAR* MappingContextPath		= TEXT("/AshDraftCore/Input/IMC_AshDefault.IMC_AshDefault");
}

AAshHeroCharacter::AAshHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No per-frame Actor Tick (ARCHITECTURE.md 18.3) — movement/anim/input are event-driven.
	PrimaryActorTick.bCanEverTick = false;

	// Player-capable hero pawns must not be AI possessed directly. Non-player heroes use
	// AAshGeneralCharacter as the explicit AI adapter.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	// The controller drives camera yaw via the spring arm, not the capsule directly.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Third-person action movement: face the direction of travel.
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, 540.f, 0.f);
		MoveComp->bConstrainToPlane = true;
		MoveComp->bSnapToPlaneAtStart = true;
		MoveComp->MaxWalkSpeed = 600.f;
		MoveComp->MinAnalogWalkSpeed = 20.f;
		MoveComp->BrakingDecelerationWalking = 2000.f;
	}

	// Camera rig: boom orbits with control rotation, camera is fixed at its end.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 500.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// GAS: single-player PoC keeps the ASC on the avatar (ARCHITECTURE.md 5).
	// Replicated so the design stays server-authority-friendly (5.5 / 15).
	AbilitySystemComponent = CreateDefaultSubobject<UAshAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAshAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AAshHeroCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UTexture2D* AAshHeroCharacter::GetAshPortrait() const
{
	return HeroConfig ? HeroConfig->Portrait.LoadSynchronous() : nullptr;
}

void AAshHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		// Watch Health so we can trigger death at zero (event-driven, ARCHITECTURE.md 15).
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAshAttributeSet::GetHealthAttribute()).AddUObject(this, &AAshHeroCharacter::OnHealthChanged);
	}
}

void AAshHeroCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAshAttributeSet::GetHealthAttribute()).RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AAshHeroCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bIsDead)
	{
		return;
	}

	// The attribute set already applied Ash.State.Dead at zero; handle the actor-side
	// consequences once here (ARCHITECTURE.md 15).
	if (Data.NewValue <= 0.f)
	{
		HandleDeath();
	}
}

void AAshHeroCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	// Cancel any in-flight abilities and tag dead (the attribute set also tags on the ASC).
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(AshGameplayTags::State_Dead);
	}

	// Stop interaction/movement (the player pawn persists for the defeat flow / camera).
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	// Play the death animation (Phase 27). Single-node, non-looping playback on the mesh holds the final
	// (downed) frame instead of blending back to idle; harmless if no anim/mesh is set.
	if (DeathAnim)
	{
		if (USkeletalMeshComponent* DeathMesh = GetMesh())
		{
			DeathMesh->PlayAnimation(DeathAnim, /*bLooping=*/false);
		}
	}

	// Notify listeners (the match loop ends as a defeat). Single-player PoC: death is terminal.
	OnHeroDied.Broadcast(this);
}

void AAshHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Authority side: bind the ASC to this avatar, seed attributes, grant abilities.
	InitializeAbilitySystem();
}

void AAshHeroCharacter::UnPossessed()
{
	Super::UnPossessed();
}

void AAshHeroCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Local-player input is available after (re)possession on the owning client.
	AddDefaultMappingContext();

	// Client side: refresh actor info so the local player's input drives the ASC.
	InitializeAbilitySystem();
}

void AAshHeroCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Owner = avatar (PoC ASC lives on the character); safe to call on both sides.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		InitializeAttributes();
		GrantDefaultAbilities();
	}
}

void AAshHeroCharacter::InitializeAttributes()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	// If a HeroConfig archetype asset is assigned, use its base stats + the player's stat bonuses.
	// The bonuses represent permanent upgrades (e.g. from a save game); they are part of the base
	// value so GE modifiers (buffs/debuffs) still layer on top correctly via the normal GAS stack.
	// Fall back to the legacy per-Blueprint Initial* floats when no config is set (backward-compat).
	float MaxHealth   = InitialMaxHealth;
	float AttackPower = InitialAttackPower;
	float Defense     = InitialDefense;
	float MaxStamina  = InitialMaxStamina;

	if (HeroConfig)
	{
		MaxHealth   = FMath::Max(1.f, HeroConfig->BaseMaxHealth   + StatBonuses.BonusMaxHealth);
		AttackPower = FMath::Max(0.f, HeroConfig->BaseAttackPower + StatBonuses.BonusAttackPower);
		Defense     = FMath::Max(0.f, HeroConfig->BaseDefense     + StatBonuses.BonusDefense);
		MaxStamina  = FMath::Max(1.f, HeroConfig->BaseMaxStamina  + StatBonuses.BonusMaxStamina);
	}

	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxHealthAttribute(),  MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetHealthAttribute(),     MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetAttackPowerAttribute(), AttackPower);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetDefenseAttribute(),    Defense);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxStaminaAttribute(), MaxStamina);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetStaminaAttribute(),    MaxStamina);
}

void AAshHeroCharacter::ApplyStatBonuses(const FAshHeroStatBonuses& NewBonuses)
{
	// No-op without a config — the legacy Initial* path has no separate bonus concept.
	if (!HeroConfig || !AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	StatBonuses = NewBonuses;

	// Re-seed the GAS base values so the new totals take effect immediately, even mid-game.
	// Current health is preserved proportionally so the player doesn't heal/die on upgrade.
	const float OldMaxHealth = FMath::Max(1.f, AbilitySystemComponent->GetNumericAttribute(UAshAttributeSet::GetMaxHealthAttribute()));
	const float HealthFraction = AbilitySystemComponent->GetNumericAttribute(UAshAttributeSet::GetHealthAttribute()) / OldMaxHealth;

	const float NewMaxHealth   = FMath::Max(1.f, HeroConfig->BaseMaxHealth   + StatBonuses.BonusMaxHealth);
	const float NewAttackPower = FMath::Max(0.f, HeroConfig->BaseAttackPower + StatBonuses.BonusAttackPower);
	const float NewDefense     = FMath::Max(0.f, HeroConfig->BaseDefense     + StatBonuses.BonusDefense);
	const float NewMaxStamina  = FMath::Max(1.f, HeroConfig->BaseMaxStamina  + StatBonuses.BonusMaxStamina);

	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxHealthAttribute(),  NewMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetHealthAttribute(),     NewMaxHealth * HealthFraction);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetAttackPowerAttribute(), NewAttackPower);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetDefenseAttribute(),    NewDefense);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxStaminaAttribute(), NewMaxStamina);
}

void AAshHeroCharacter::GrantDefaultAbilities()
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

		// Stamp the ability's input tag onto the spec so input routing can find it.
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

void AAshHeroCharacter::ReceiveSoldierDamage(float Amount, AActor* /*DamageInstigator*/)
{
	if (bIsDead || Amount <= 0.f || !AbilitySystemComponent)
	{
		return;
	}

	// Mirror the general's path: drive Health directly on the attribute set. The health-changed delegate
	// raises HandleDeath at zero, so this stays consistent with GAS-applied damage (the defeat flow).
	const float CurrentHealth = AbilitySystemComponent->GetNumericAttribute(UAshAttributeSet::GetHealthAttribute());
	AbilitySystemComponent->SetNumericAttributeBase(
		UAshAttributeSet::GetHealthAttribute(),
		FMath::Max(0.f, CurrentHealth - Amount));
}

float AAshHeroCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AAshHeroCharacter::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}

float AAshHeroCharacter::GetHealthNormalized() const
{
	const float Max = GetMaxHealth();
	return Max > 0.f ? GetHealth() / Max : 0.f;
}

void AAshHeroCharacter::AddDefaultMappingContext()
{
	if (!DefaultMappingContext)
	{
		return;
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, DefaultMappingContextPriority);
	}
}

void AAshHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput || !InputConfig)
	{
		return;
	}

	// Movement.
	if (const UInputAction* MoveAction = InputConfig->FindNativeInputActionForTag(AshGameplayTags::InputTag_Move))
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAshHeroCharacter::Input_Move);
	}

	// Look (mouse + gamepad stick share one handler).
	if (const UInputAction* LookMouseAction = InputConfig->FindNativeInputActionForTag(AshGameplayTags::InputTag_Look_Mouse))
	{
		EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &AAshHeroCharacter::Input_Look);
	}
	if (const UInputAction* LookStickAction = InputConfig->FindNativeInputActionForTag(AshGameplayTags::InputTag_Look_Stick))
	{
		EnhancedInput->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AAshHeroCharacter::Input_Look);
	}

	// Ability inputs: each AbilityInputActions entry routes to GAS by its tag
	// (Lyra-style data-driven binding). Pressed activates, released ends.
	for (const FAshInputAction& AbilityInput : InputConfig->AbilityInputActions)
	{
		if (!AbilityInput.InputAction || !AbilityInput.InputTag.IsValid())
		{
			continue;
		}

		EnhancedInput->BindAction(AbilityInput.InputAction, ETriggerEvent::Started,
			this, &AAshHeroCharacter::Input_AbilityTagPressed, AbilityInput.InputTag);
		EnhancedInput->BindAction(AbilityInput.InputAction, ETriggerEvent::Completed,
			this, &AAshHeroCharacter::Input_AbilityTagReleased, AbilityInput.InputTag);
	}
}

void AAshHeroCharacter::Input_AbilityTagPressed(FGameplayTag InputTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void AAshHeroCharacter::Input_AbilityTagReleased(FGameplayTag InputTag)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagReleased(InputTag);
	}
}

void AAshHeroCharacter::Input_Move(const FInputActionValue& Value)
{
	AController* MovementController = GetController();
	if (!MovementController)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();

	// Move relative to the camera's yaw so movement is screen-relative (third-person).
	const FRotator YawRotation(0.f, MovementController->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Axis.Y);
	AddMovementInput(RightDirection, Axis.X);
}

void AAshHeroCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}
