// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "UI/AshUserWidget.h"
#include "UI/AshUITypes.h"
#include "AshHUDWidget.generated.h"

class UAshAbilitySystemComponent;
class UAshTargetInfoWidget;
class UImage;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

/**
 * UAshHUDWidget
 *
 * The player HUD root (PLAN Phase 30, ARCHITECTURE.md 16). It observes the local hero's GAS attributes
 * and the combat feed subsystem and draws:
 *   - bottom-centre: the hero's health bar with a mana bar below it (mana = the Stamina attribute, the
 *     project's secondary resource pool — there is no separate Mana attribute);
 *   - bottom-right: the player's kill count;
 *   - top-left: a recently-struck-enemy panel (name / affiliation / health) via UAshTargetInfoWidget.
 *
 * Smooth bars: each bar's displayed percent is interpolated toward its target every frame (NativeTick),
 * so health/mana glide down on damage instead of snapping. An optional trailing "chip" bar (HealthBarTrail
 * / ManaBarTrail) lags further behind to highlight the amount just lost — the decrease animation the brief
 * asks for. Widget tick is the idiomatic place for this UI lerp (it is not Actor Tick — ARCHITECTURE.md
 * 18.3 governs gameplay actors, not view interpolation).
 *
 * It reads state, never mutates gameplay (ARCHITECTURE.md 16): attribute changes arrive through the ASC
 * change delegates and combat events through the subsystem's BlueprintAssignable delegates. Author a WBP
 * child of this class and name the inner widgets to bind them (all bindings optional, so the layout is
 * free).
 */
UCLASS(Abstract)
class ASHDRAFTCORERUNTIME_API UAshHUDWidget : public UAshUserWidget
{
	GENERATED_BODY()

protected:
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~End of UUserWidget interface

	// --- Binding ---

	/** Finds the local hero + its ASC and binds the attribute-change delegates. Returns true on success. */
	bool TryBindToHero();

	/** Binds the combat feed subsystem delegates (kill count / struck unit) and seeds initial values. */
	void BindToCombatFeed();

	/** Re-reads the four target fractions from the bound ASC (health/mana current over max). */
	void RefreshTargetsFromASC();

	/** ASC attribute-change handler: any of the four tracked attributes recomputes the targets. */
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);

	/** Combat feed handler: refresh the kill-count text. */
	UFUNCTION()
	void HandleKillCountChanged(int32 NewKillCount);

	/** Combat feed handler: push the struck unit into the recently-struck panel + reveal it. */
	UFUNCTION()
	void HandleUnitStruck(const FAshStruckUnitInfo& Info);

	/** Writes Count into KillCountText using KillCountFormat. */
	void UpdateKillCountText(int32 Count);

	// --- Blueprint polish hooks ---

	/** Fired when the kill count changes (e.g. to play a pop animation). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ash|UI")
	void OnKillCountChanged(int32 NewKillCount);

	/** Fired when the player strikes a unit (e.g. to fade the panel in / restart a hide timer). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ash|UI")
	void OnUnitStruck(const FAshStruckUnitInfo& Info);

	// --- Bound widgets (all optional so the WBP layout is free) ---

	/** Hero health bar (front layer; interpolates toward the live fraction). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	/** Optional trailing health chip bar (lags behind to show the amount just lost). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBarTrail;

	/** Hero mana bar (the Stamina attribute; front layer). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ManaBar;

	/** Optional trailing mana chip bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ManaBarTrail;

	/** Bottom-right kill counter. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KillCountText;

	/** Top-left recently-struck-enemy panel. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UAshTargetInfoWidget> RecentTargetPanel;

	/**
	 * Optional hero portrait image. When the bound hero has a UAshHeroConfig with a Portrait texture,
	 * this image is filled automatically when the HUD first binds to the hero.
	 * Name the inner widget exactly "Protrait" in the WBP to activate the binding.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Protrait;

	// --- Tunables ---

	/** Interpolation speed of the front bars toward their target (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|UI", meta = (ClampMin = "0.1"))
	float BarInterpSpeed = 9.f;

	/** Interpolation speed of the trailing chip bars (lower = a longer-lingering chip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|UI", meta = (ClampMin = "0.1"))
	float TrailInterpSpeed = 2.5f;

	/** Format for the kill counter; {0} is the count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|UI")
	FText KillCountFormat = FText::FromString(TEXT("Kills: {0}"));

	/** Hide the recently-struck panel until the first strike. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|UI")
	bool bHideTargetPanelUntilFirstStrike = true;

private:
	/** ASC we bound attribute delegates to (weak: never keep the avatar alive). */
	TWeakObjectPtr<UAshAbilitySystemComponent> BoundASC;

	/** True once the combat feed delegates are bound. */
	bool bBoundToFeed = false;

	// Target (live) and displayed (interpolated) bar fractions, 0-1.
	float HealthTarget = 1.f;
	float HealthDisplay = 1.f;
	float HealthTrailDisplay = 1.f;
	float ManaTarget = 1.f;
	float ManaDisplay = 1.f;
	float ManaTrailDisplay = 1.f;
};
