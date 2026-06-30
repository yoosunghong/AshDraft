// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "UI/AshUserWidget.h"
#include "Teams/AshTeamTypes.h"
#include "AshUnitHealthBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * UAshUnitHealthBarWidget
 *
 * The over-head health bar for friendly/enemy soldiers and generals (PLAN Phase 30 "Unit Health UI").
 * Friendly bars are cyan, enemy bars red (UAshUIStatics::GetTeamHealthColor). It is a dumb view: the
 * owner (the soldier proxy's UWidgetComponent, or a general's widget component) calls SetTeam once and
 * SetHealth each update.
 *
 * It rides the existing representation LOD: the soldier proxy only exists (and renders this bar) while
 * the entity is promoted to an Actor near the player (LOD <= the representation processor's
 * PromoteAtOrBelowLOD), so a bar appears "together with the mesh when it is rendered" and far units pay
 * no widget cost. Author a WBP child of this class with a UProgressBar named HealthBar.
 */
UCLASS(Abstract)
class ASHDRAFTCORERUNTIME_API UAshUnitHealthBarWidget : public UAshUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the displayed health fraction (0-1). */
	UFUNCTION(BlueprintCallable, Category = "Ash|UI")
	virtual void SetHealth(float Fraction);

	/** Sets the owning unit's team and recolours the bar (cyan friendly / red enemy). */
	UFUNCTION(BlueprintCallable, Category = "Ash|UI")
	virtual void SetTeam(EAshTeamId InTeam);

	/** Sets the unit name shown above the health bar. Pass empty text to hide. */
	UFUNCTION(BlueprintCallable, Category = "Ash|UI")
	void SetUnitName(const FText& InName);

protected:
	virtual void NativeConstruct() override;

	/** Applies the current team's fill colour to the bar. */
	virtual void ApplyTeamColor();

	/** Shows the optional label only after a non-empty name has been explicitly configured. */
	void ApplyUnitNameVisibility();

	/** The unit's health bar (0-1). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	/** Optional name label shown above the bar. Name the inner widget "UnitNameText" in the WBP. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UnitNameText;

	/** Cached team; drives the fill colour. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI")
	EAshTeamId Team = EAshTeamId::Neutral;

	/** True only after gameplay/data has supplied a non-empty unit name. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Ash|UI")
	bool bHasConfiguredUnitName = false;
};
