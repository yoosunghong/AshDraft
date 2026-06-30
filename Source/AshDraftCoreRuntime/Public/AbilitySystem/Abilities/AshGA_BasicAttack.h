// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AbilitySystem/AshGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "AshGA_BasicAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UGameplayEffect;
struct FGameplayEventData;

/**
 * UAshGA_BasicAttack
 *
 * The hero's basic melee attack (ARCHITECTURE.md 5.4, GA_Hero_BasicAttack).
 * On activation it sweeps a short capsule in front of the avatar and applies the
 * damage Gameplay Effect to any hit unit that owns an ability system component.
 *
 * Combo: ComboMontages is an ordered array (the PoC uses 4). The ability stays
 * active for the whole chain. Each montage carries a UAshAnimNotifyState_ComboWindow
 * marking when a repeat press may cancel the current swing into the next hit —
 * this is what removes the slow recovery "after delay": pressing inside the window
 * interrupts the montage immediately and plays the next. A press before the window
 * (wind-up) is buffered and fires the instant the window opens. If a montage plays
 * to its end with no cancel, the combo ends (next press restarts at step 0).
 *
 * Damage timing: each combo montage also carries a UAshAnimNotify_MeleeHit at its
 * contact frame (HitEventTag); the ability sweeps for damage when that fires.
 *
 * With no montages assigned it falls back to a single instant sweep so the
 * ability still works before animations exist.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshGA_BasicAttack : public UAshGameplayAbility
{
	GENERATED_BODY()

public:
	UAshGA_BasicAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Plays the combo montage at Index (interrupting any current one) and tracks it. */
	void PlayComboMontage(int32 Index);

	/** Cancels the current montage and plays the next combo step, if any. */
	void AdvanceCombo();

	/** Detaches + ends the current montage task so its interrupt won't end the ability. */
	void StopCurrentMontageTask();

	/** Sweeps for targets in front of the avatar and applies the damage effect. */
	void PerformHitSweep();

	/** Hit-event handler bound to the montage's melee-hit notify (per montage). */
	UFUNCTION()
	void OnHitEventReceived(FGameplayEventData Payload);

	/** Repeat-input handler: cancels into the next hit if the window is open, else buffers. */
	UFUNCTION()
	void OnComboInputReceived(FGameplayEventData Payload);

	/** Combo cancel window opened (from the montage notify state). */
	UFUNCTION()
	void OnComboWindowOpened(FGameplayEventData Payload);

	/** Combo cancel window closed. */
	UFUNCTION()
	void OnComboWindowClosed(FGameplayEventData Payload);

	/** Current montage finished naturally with no cancel; the combo ends. */
	UFUNCTION()
	void OnComboMontageEnded();

	/** Current montage was interrupted/cancelled externally; end the combo. */
	UFUNCTION()
	void OnComboMontageInterrupted();

	/** Ordered combo montages (PoC: 4). Each should carry Ash Melee Hit + Ash Combo Window notifies. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack|Combo")
	TArray<TObjectPtr<UAnimMontage>> ComboMontages;

	/**
	 * Dash Attack montage (Phase 32): played as the opening hit instead of ComboMontages[0] when the hero
	 * pressed attack after moving continuously for AAshHeroCharacter::DashAttackMoveSeconds. Should carry the
	 * same Ash Melee Hit (+ optional Ash Combo Window to chain into ComboMontages[1]) notifies. Optional —
	 * with none assigned the normal combo opener is used even on a dash.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack|Combo")
	TObjectPtr<UAnimMontage> DashAttackMontage;

	/**
	 * Forward lunge speed (cm/s) applied as the hero starts each combo hit so the attack steps forward
	 * (Phase 32). Applied via LaunchCharacter; the step is brief because ground braking decelerates it. 0
	 * disables the lunge. NOTE: if an attack montage uses (in-place) root motion it will override this — turn
	 * the montage's root motion off, or author its root motion to move forward, for the step to show.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack", meta = (ClampMin = "0.0"))
	float AttackLungeSpeed = 500.f;

	/** Forward lunge speed (cm/s) for the dash-attack opener — usually larger than AttackLungeSpeed. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack", meta = (ClampMin = "0.0"))
	float DashAttackLungeSpeed = 1000.f;

	/** Play rate applied to combo montages (raise to speed the whole combo up). */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack|Combo", meta = (ClampMin = "0.1"))
	float ComboMontagePlayRate = 1.f;

	/** Repeat-input event (sent by the ASC) that advances the combo. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack|Combo", meta = (Categories = "Ash.Event"))
	FGameplayTag ComboInputEventTag;

	/** Cancel-window-open event (sent by the montage's Ash Combo Window notify). */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack|Combo", meta = (Categories = "Ash.Event"))
	FGameplayTag ComboWindowOpenEventTag;

	/** Cancel-window-close event. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack|Combo", meta = (Categories = "Ash.Event"))
	FGameplayTag ComboWindowCloseEventTag;

	/** Damage effect applied to each valid target hit by the sweep. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Reach of the melee sweep from the avatar's location (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack", meta = (ClampMin = "0.0"))
	float AttackRange = 175.f;

	/** Radius of the melee sweep sphere (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack", meta = (ClampMin = "0.0"))
	float AttackRadius = 90.f;

	/** Gameplay event (from the montage's melee-hit notify) that triggers the sweep. */
	UPROPERTY(EditDefaultsOnly, Category = "Ash|Attack", meta = (Categories = "Ash.Event"))
	FGameplayTag HitEventTag;

private:
	/** Index of the combo step currently playing (0-based). */
	int32 ComboIndex = 0;

	/** A press arrived before the window; chain the next step the moment it opens. */
	bool bComboInputBuffered = false;

	/** True while the current montage's cancel window is open. */
	bool bComboWindowOpen = false;

	/** This activation opened with the dash attack (so PlayComboMontage(0) uses DashAttackMontage). */
	bool bDashAttack = false;

	/**
	 * True once the current combo step has run its damage sweep (via its melee-hit notify). If a step is
	 * left (cancelled into the next, or the montage ends) without having swept — e.g. a montage with no Ash
	 * Melee Hit notify authored — the ability sweeps once as a fallback so EVERY combo hit deals damage.
	 */
	bool bStepSwept = false;

	/** The montage task for the step currently playing (kept so we can detach it on cancel). */
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> CurrentMontageTask;
};
