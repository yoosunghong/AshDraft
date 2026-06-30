// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "UI/AshUnitHealthBarWidget.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "AshSoldierHealthBarWidget.generated.h"

class SProgressBar;

/**
 * Native, compact over-head health bar for ordinary Mass soldiers.
 *
 * This intentionally does not use the general/hero WBP layout: a soldier bar is a tiny tactical marker,
 * so its desired size is owned here and cannot accidentally inherit a hero-sized Widget Blueprint canvas.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshSoldierHealthBarWidget : public UAshUnitHealthBarWidget
{
	GENERATED_BODY()

public:
	virtual void SetHealth(float Fraction) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void ApplyTeamColor() override;

	/** Desired widget width in screen-space pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|UI", meta = (ClampMin = "1.0"))
	float SoldierBarWidth = 30.f;

	/** Desired widget height in screen-space pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|UI", meta = (ClampMin = "1.0"))
	float SoldierBarHeight = 6.f;

private:
	float HealthFraction = 1.f;
	FProgressBarStyle ProgressBarStyle;
	TSharedPtr<SProgressBar> SlateHealthBar;
};
