// Copyright YoosungHong. All Rights Reserved.

#include "UI/AshTargetInfoWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UAshTargetInfoWidget::SetTargetInfo(const FAshStruckUnitInfo& Info)
{
	if (UnitNameText)
	{
		UnitNameText->SetText(Info.UnitName);
	}

	if (AffiliationText)
	{
		AffiliationText->SetText(UAshUIStatics::GetTeamDisplayText(Info.Team));
		if (bTintAffiliationByTeam)
		{
			AffiliationText->SetColorAndOpacity(FSlateColor(UAshUIStatics::GetTeamHealthColor(Info.Team)));
		}
	}

	if (HealthBar)
	{
		HealthBar->SetPercent(FMath::Clamp(Info.HealthFraction, 0.f, 1.f));
		if (bTintHealthBarByTeam)
		{
			HealthBar->SetFillColorAndOpacity(UAshUIStatics::GetTeamHealthColor(Info.Team));
		}
	}

	if (Protrait)
	{
		if (Info.Portrait)
		{
			Protrait->SetBrushFromTexture(Info.Portrait, /*bMatchSize=*/true);
			Protrait->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Protrait->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	OnTargetInfoUpdated(Info);
}
