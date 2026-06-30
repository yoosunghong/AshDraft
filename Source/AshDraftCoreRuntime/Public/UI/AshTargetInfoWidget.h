// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "UI/AshUserWidget.h"
#include "UI/AshUITypes.h"
#include "AshTargetInfoWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

/**
 * UAshTargetInfoWidget
 *
 * The "recently struck enemy" panel shown top-left of the HUD (PLAN Phase 30): the struck unit's
 * name, affiliation, and a health bar. It is a dumb view — UAshHUDWidget owns it as an optional child
 * widget and pushes a snapshot in via SetTargetInfo whenever the player strikes something.
 *
 * Author a WBP child of this class and (optionally) name the inner widgets UnitNameText /
 * AffiliationText / HealthBar so they bind automatically. The bindings are optional, so a designer can
 * omit any element or restyle freely; SetTargetInfo guards every pointer.
 */
UCLASS(Abstract)
class ASHDRAFTCORERUNTIME_API UAshTargetInfoWidget : public UAshUserWidget
{
	GENERATED_BODY()

public:
	/** Fills the name / affiliation / health-bar from a struck-unit snapshot. Colours by team. */
	UFUNCTION(BlueprintCallable, Category = "Ash|UI")
	void SetTargetInfo(const FAshStruckUnitInfo& Info);

	/** Blueprint polish hook fired by SetTargetInfo (e.g. restart a fade/slide-in animation). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ash|UI")
	void OnTargetInfoUpdated(const FAshStruckUnitInfo& Info);

protected:
	/** Struck unit's display name. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UnitNameText;

	/** Struck unit's affiliation ("Enemy" etc.). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AffiliationText;

	/** Struck unit's health bar (0-1). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	/** Optional portrait image for the struck unit. Name the inner widget "Protrait" in the WBP. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Protrait;

	/** When true, tint the health bar by the struck unit's team (cyan ally / red enemy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|UI")
	bool bTintHealthBarByTeam = true;

	/** When true, tint the affiliation label by the struck unit's team. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|UI")
	bool bTintAffiliationByTeam = true;
};
