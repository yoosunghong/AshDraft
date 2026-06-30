// Copyright YoosungHong. All Rights Reserved.

#include "UI/AshSoldierHealthBarWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Styling/SlateBrush.h"
#include "UI/AshUITypes.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"

TSharedRef<SWidget> UAshSoldierHealthBarWidget::RebuildWidget()
{
	const FLinearColor BackColor(0.f, 0.f, 0.f, 0.55f);

	ProgressBarStyle = FProgressBarStyle()
		.SetBackgroundImage(FSlateColorBrush(BackColor))
		.SetFillImage(FSlateColorBrush(UAshUIStatics::GetTeamHealthColor(Team)))
		.SetMarqueeImage(FSlateNoResource());

	SAssignNew(SlateHealthBar, SProgressBar)
		.Style(&ProgressBarStyle)
		.Percent(HealthFraction)
		.FillColorAndOpacity(UAshUIStatics::GetTeamHealthColor(Team));

	return SNew(SBox)
		.WidthOverride(SoldierBarWidth)
		.HeightOverride(SoldierBarHeight)
		[
			SlateHealthBar.ToSharedRef()
		];
}

void UAshSoldierHealthBarWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SlateHealthBar.Reset();
}

void UAshSoldierHealthBarWidget::SetHealth(float Fraction)
{
	HealthFraction = FMath::Clamp(Fraction, 0.f, 1.f);
	if (SlateHealthBar)
	{
		SlateHealthBar->SetPercent(HealthFraction);
	}
}

void UAshSoldierHealthBarWidget::ApplyTeamColor()
{
	const FLinearColor TeamColor = UAshUIStatics::GetTeamHealthColor(Team);
	ProgressBarStyle.SetFillImage(FSlateColorBrush(TeamColor));
	if (SlateHealthBar)
	{
		SlateHealthBar->SetFillColorAndOpacity(TeamColor);
	}
}
