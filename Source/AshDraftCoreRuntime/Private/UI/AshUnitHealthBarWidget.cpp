// Copyright YoosungHong. All Rights Reserved.

#include "UI/AshUnitHealthBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "UI/AshUITypes.h"

void UAshUnitHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyTeamColor();
	ApplyUnitNameVisibility();
}

void UAshUnitHealthBarWidget::SetHealth(float Fraction)
{
	if (HealthBar)
	{
		HealthBar->SetPercent(FMath::Clamp(Fraction, 0.f, 1.f));
	}
}

void UAshUnitHealthBarWidget::SetTeam(EAshTeamId InTeam)
{
	if (Team != InTeam)
	{
		Team = InTeam;
		ApplyTeamColor();
	}
}

void UAshUnitHealthBarWidget::ApplyTeamColor()
{
	if (HealthBar)
	{
		HealthBar->SetFillColorAndOpacity(UAshUIStatics::GetTeamHealthColor(Team));
	}
}

void UAshUnitHealthBarWidget::SetUnitName(const FText& InName)
{
	bHasConfiguredUnitName = !InName.IsEmpty();
	if (UnitNameText)
	{
		UnitNameText->SetText(InName);
		ApplyUnitNameVisibility();
	}
}

void UAshUnitHealthBarWidget::ApplyUnitNameVisibility()
{
	if (UnitNameText)
	{
		UnitNameText->SetVisibility(bHasConfiguredUnitName ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
