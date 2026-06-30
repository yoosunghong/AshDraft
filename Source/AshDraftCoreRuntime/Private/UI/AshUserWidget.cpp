// Copyright YoosungHong. All Rights Reserved.

#include "UI/AshUserWidget.h"

#include "Character/AshHeroCharacter.h"

AAshHeroCharacter* UAshUserWidget::GetLocalHero() const
{
	return Cast<AAshHeroCharacter>(GetOwningPlayerPawn());
}
