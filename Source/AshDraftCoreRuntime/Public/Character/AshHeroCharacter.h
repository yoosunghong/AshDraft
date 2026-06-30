// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Hero/AshHeroProgressionData.h"
#include "Teams/AshTeamTypes.h"
#include "Teams/AshTeamAgentInterface.h"
#include "AshHeroCharacter.generated.h"

class UAshAbilitySystemComponent;
class UAshAttributeSet;
class UAshGameplayAbility;
class UAshHeroConfig;
class UAshInputConfig;
class UGameplayEffect;
class UAnimSequenceBase;
class UCameraComponent;
class UInputMappingContext;
class USpringArmComponent;
class UTexture2D;
struct FGameplayTag;
struct FInputActionValue;
struct FOnAttributeChangeData;

/** Broadcast once when the hero's health first reaches zero (event-driven death). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAshOnHeroDied, AAshHeroCharacter*, Hero);

/**
 * AAshHeroCharacter
 *
 * The first playable general/hero (ARCHITECTURE.md 6.1). A high-fidelity
 * third-person ACharacter that owns its camera rig and consumes the project's
 * data-driven Enhanced Input config (UAshInputConfig) for movement and look.
 *
 * Phase 3 scope: movement, camera, Enhanced Input, placeholder animation,
 * basic team identity, and placeholder health values. GAS-backed combat,
 * abilities, and a real AttributeSet are introduced in Phase 4, at which point
 * the Health/MaxHealth placeholders here are replaced by GAS attributes.
 *
 * Spawned through the Lyra experience/PawnData pipeline: it is a plain
 * ACharacter (PawnData.PawnClass accepts any APawn) and stays decoupled from
 * LyraGame internals so the AshDraftCore plugin owns its own input/camera setup.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshHeroCharacter : public ACharacter, public IAbilitySystemInterface, public IAshTeamAgentInterface
{
	GENERATED_BODY()

public:
	AAshHeroCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	//~APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	//~End of APawn interface

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	/** Hero archetype asset. May be null when using the legacy Initial* workflow. */
	UFUNCTION(BlueprintPure, Category = "Ash|Hero")
	UAshHeroConfig* GetHeroConfig() const { return HeroConfig; }

	/**
	 * Applies (or replaces) the player's stat bonuses and re-seeds GAS base values immediately.
	 * Call this after loading a save game to reflect the player's current progression without
	 * needing to re-possess the character.  Requires HeroConfig to be set; does nothing otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ash|Hero")
	void ApplyStatBonuses(const FAshHeroStatBonuses& NewBonuses);

	/** Current team this hero belongs to (placeholder identity until GAS/team subsystem in later phases). */
	UFUNCTION(BlueprintPure, Category = "Ash|Team")
	EAshTeamId GetTeamId() const { return TeamId; }

	/** Sets the hero's team identity. */
	UFUNCTION(BlueprintCallable, Category = "Ash|Team")
	void SetTeamId(EAshTeamId NewTeamId) { TeamId = NewTeamId; }

	//~IAshTeamAgentInterface
	virtual EAshTeamId GetAshTeamId() const override { return TeamId; }
	virtual FText GetAshDisplayName() const override { return DisplayName; }
	virtual UTexture2D* GetAshPortrait() const override;
	virtual void ApplyHitReaction(const FVector& SourceLocation, int32 SourceId) override;
	//~End of IAshTeamAgentInterface

	/** True while the hero is stunned (the Ash.State.Stunned tag is present): movement and attacks blocked. */
	UFUNCTION(BlueprintPure, Category = "Ash|Combat")
	bool IsStunned() const;

	/**
	 * True if the next basic attack should be the Dash Attack variant: the hero has been moving
	 * continuously for at least DashAttackMoveSeconds when the attack is pressed (Phase 32). Read by
	 * UAshGA_BasicAttack on activation to pick its dash-attack montage.
	 */
	UFUNCTION(BlueprintPure, Category = "Ash|Combat")
	bool ShouldUseDashAttack() const;

	/** Current health, read from the GAS AttributeSet. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetHealth() const;

	/** Maximum health, read from the GAS AttributeSet. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetMaxHealth() const;

	/** Health as a 0-1 fraction; safe against a zero MaxHealth. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetHealthNormalized() const;

	/** True once health has reached zero and death has been handled. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	bool IsDead() const { return bIsDead; }

	/**
	 * Applies melee damage to the hero from a Mass soldier strike. Mirrors the general's entry point so
	 * the Mass combat processor can damage the player the same way it damages a general (the soldier is a
	 * Mass entity with no GAS context of its own). Drives Health straight on the attribute set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ash|Health")
	void ReceiveSoldierDamage(float Amount, AActor* DamageInstigator);

	/** Fired when the hero dies. The match loop binds here for the player-death defeat. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Health")
	FAshOnHeroDied OnHeroDied;

protected:
	/** Health-changed handler: triggers death at zero. */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/** One-time death handling: cancels abilities, disables input/collision, broadcasts. */
	void HandleDeath();

	/**
	 * Applies the stun state for Duration seconds (Phase 32), honoring the game-wide new-source immunity
	 * (UAshCombatRulesSettings): the same attacker may re-stun freely (a combo) but a *different* attacker is
	 * rejected until the immunity window elapses. When allowed it applies GE_State_Stunned (granting
	 * Ash.State.Stunned for Duration via SetByCaller) and cancels the in-flight attack. SourceId identifies
	 * the attacker (its UniqueID, or an attacker Mass entity index for soldier hits).
	 */
	void ApplyStun(float Duration, int32 SourceId);

	/** Move input handler: translates a 2D axis into world movement relative to control yaw. */
	void Input_Move(const FInputActionValue& Value);

	/** Look input handler: applies a 2D axis to controller yaw/pitch. */
	void Input_Look(const FInputActionValue& Value);

	/** Adds the default mapping context to the local player's Enhanced Input subsystem. */
	void AddDefaultMappingContext();

	/** Initializes the ability system actor info, applies initial attributes, and grants abilities. */
	void InitializeAbilitySystem();

	/** Pushes the editor-tunable initial stats into the attribute set base values. */
	void InitializeAttributes();

	/** Grants the configured default abilities (authority only). */
	void GrantDefaultAbilities();

	/** Forwards an ability input press to the ability system by input tag. */
	void Input_AbilityTagPressed(FGameplayTag InputTag);

	/** Forwards an ability input release to the ability system by input tag. */
	void Input_AbilityTagReleased(FGameplayTag InputTag);


private:
	/** Third-person camera boom; uses pawn control rotation so look input orbits the hero. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Follow camera at the end of the boom. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/**
	 * Hero archetype asset (DA_Hero_*).  Defines base stats shared by the player-controlled instance
	 * and any AI-controlled (enemy) version of the same hero.  When set, base values in this asset
	 * take precedence over the legacy Initial* floats below; FAshHeroStatBonuses are added on top.
	 * Leave null to keep the old per-Blueprint Initial* workflow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAshHeroConfig> HeroConfig;

	/**
	 * Player progression bonuses applied on top of HeroConfig base stats.
	 * In the PoC set these directly on the hero Blueprint / instance to simulate upgrades.
	 * In a shipping game, load from a USaveGame and call ApplyStatBonuses() after possession.
	 * Has no effect when HeroConfig is null (the legacy Initial* floats are used instead).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|Hero", meta = (AllowPrivateAccess = "true"))
	FAshHeroStatBonuses StatBonuses;

	/** Data-driven Input Action <-> tag map (Lyra-style). Move/Look are read from NativeInputActions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UAshInputConfig> InputConfig;

	/** Enhanced Input mapping context applied when this hero is controlled by a local player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Priority of DefaultMappingContext within the Enhanced Input subsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Input", meta = (AllowPrivateAccess = "true"))
	int32 DefaultMappingContextPriority = 0;

	/** Team identity (data-tunable on the instance/Blueprint). Defaults to the player team. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Team", meta = (AllowPrivateAccess = "true"))
	EAshTeamId TeamId = EAshTeamId::Player;

	/** Display name for UI (Phase 30). Empty falls back to a generic label. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UI", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** GAS ability system component owned by this hero (single-player PoC: ASC on the avatar). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAshAbilitySystemComponent> AbilitySystemComponent;

	/** Combat attribute set (Health/AttackPower/Defense/Stamina). */
	UPROPERTY()
	TObjectPtr<UAshAttributeSet> AttributeSet;

	/** Abilities granted on spawn (authority). Each carries the input tag it binds to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UAshGameplayAbility>> DefaultAbilities;

	/**
	 * Animation played once when the hero dies (Phase 27). A raw AnimSequence played in single-node mode
	 * so it holds the final (downed) frame instead of blending back to idle. The player pawn is not
	 * auto-removed (death is terminal defeat — the match flow owns the player pawn). Optional (null = none).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimSequenceBase> DeathAnim;

	// Editor-tunable initial attribute values (ARCHITECTURE.md 17). Pushed into the
	// attribute set base values when the ability system initializes.

	/** Initial (and maximum) health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxHealth = 100.f;

	/** Initial attack power fed into the damage execution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InitialAttackPower = 25.f;

	/** Initial defense subtracted from incoming damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InitialDefense = 5.f;

	/** Initial (and maximum) stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxStamina = 100.f;

	// --- Hit reaction / stun (Phase 32) ---

	/** Seconds the hero is stunned (no move/attack) when struck. 0 disables the hero's stun entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Combat|HitReact", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float HitReactStunDuration = 0.35f;

	/** Speed (cm/s) of the slight knockback launch away from the attacker on being hit. 0 = no knockback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Combat|HitReact", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float HitReactKnockbackSpeed = 350.f;

	/** Stun Gameplay Effect applied on being hit (GE_State_Stunned). The new-source immunity rule still gates it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Combat|HitReact", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> StunEffectClass;

	// --- Attack-move tracking (Phase 32: dash attack after sustained movement) ---

	/** Continuous-movement seconds required before a basic attack becomes the Dash Attack variant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Combat|HitReact", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DashAttackMoveSeconds = 2.f;

	/** Gap (s) without a move input after which movement is considered to have stopped (resets the run timer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Combat|HitReact", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MoveInputResetGap = 0.15f;

	/** World time movement (continuous move input) began; the run length is Now - this. */
	float MoveInputStartTime = -1.e30f;

	/** World time of the most recent move input; used to detect when movement stopped. */
	float LastMoveInputTime = -1.e30f;

	/** Identifier of the attacker that last stunned the hero (UniqueID / Mass entity index). */
	int32 LastStunSourceId = INDEX_NONE;

	/** World time (s) of the last applied stun; the new-source immunity window is measured from here. */
	float LastStunTime = -1.e30f;

	/** Guards one-time ability granting across PossessedBy / BeginPlay. */
	bool bAbilitiesGranted = false;

	/** Guards one-time death handling. */
	bool bIsDead = false;
};
