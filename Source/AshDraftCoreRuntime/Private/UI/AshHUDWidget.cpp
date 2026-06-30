// Copyright YoosungHong. All Rights Reserved.

#include "UI/AshHUDWidget.h"

#include "AbilitySystem/AshAbilitySystemComponent.h"
#include "AbilitySystem/AshAttributeSet.h"
#include "Character/AshHeroCharacter.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Hero/AshHeroConfig.h"
#include "UI/AshCombatFeedSubsystem.h"
#include "UI/AshTargetInfoWidget.h"

void UAshHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The hero may not be possessed yet when the HUD is created; TryBindToHero retries from NativeTick.
	TryBindToHero();
	BindToCombatFeed();

	if (bHideTargetPanelUntilFirstStrike && RecentTargetPanel)
	{
		const UAshCombatFeedSubsystem* Feed = UAshCombatFeedSubsystem::Get(this);
		if (!Feed || !Feed->GetLastStruckUnit().bValid)
		{
			RecentTargetPanel->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UAshHUDWidget::NativeDestruct()
{
	if (UAshAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UAshAttributeSet::GetHealthAttribute()).RemoveAll(this);
		ASC->GetGameplayAttributeValueChangeDelegate(UAshAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
		ASC->GetGameplayAttributeValueChangeDelegate(UAshAttributeSet::GetStaminaAttribute()).RemoveAll(this);
		ASC->GetGameplayAttributeValueChangeDelegate(UAshAttributeSet::GetMaxStaminaAttribute()).RemoveAll(this);
	}
	BoundASC.Reset();

	if (UAshCombatFeedSubsystem* Feed = UAshCombatFeedSubsystem::Get(this))
	{
		Feed->OnPlayerKillCountChanged.RemoveAll(this);
		Feed->OnUnitStruck.RemoveAll(this);
	}

	Super::NativeDestruct();
}

bool UAshHUDWidget::TryBindToHero()
{
	if (BoundASC.IsValid())
	{
		return true;
	}

	AAshHeroCharacter* Hero = GetLocalHero();
	UAshAbilitySystemComponent* ASC = Hero ? Cast<UAshAbilitySystemComponent>(Hero->GetAbilitySystemComponent()) : nullptr;
	if (!ASC)
	{
		return false;
	}

	BoundASC = ASC;

	// One handler for all four attributes; each change recomputes the health/mana target fractions.
	ASC->GetGameplayAttributeValueChangeDelegate(UAshAttributeSet::GetHealthAttribute()).AddUObject(this, &UAshHUDWidget::HandleAttributeChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(UAshAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UAshHUDWidget::HandleAttributeChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(UAshAttributeSet::GetStaminaAttribute()).AddUObject(this, &UAshHUDWidget::HandleAttributeChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(UAshAttributeSet::GetMaxStaminaAttribute()).AddUObject(this, &UAshHUDWidget::HandleAttributeChanged);

	// Seed the bars at the current values so they start filled instead of lerping up from zero.
	// Guard: if InitAbilityActorInfo hasn't run yet, SpawnedAttributes is empty and GetNumericAttribute
	// returns 0 — snapping HealthDisplay to 0 would make the bars flash empty on spawn. Keep the
	// 1.0 defaults and let the first attribute-change delegate (fired by InitializeAttributes) correct
	// them; the widget will retry via NativeTick until it sees valid data.
	RefreshTargetsFromASC();
	const float MaxHealth = ASC->GetNumericAttribute(UAshAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth > SMALL_NUMBER)
	{
		HealthDisplay = HealthTrailDisplay = HealthTarget;
		ManaDisplay   = ManaTrailDisplay   = ManaTarget;
	}

	// Fill the hero portrait if the archetype config supplies one.
	if (Protrait)
	{
		if (const UAshHeroConfig* Config = Hero->GetHeroConfig())
		{
			if (UTexture2D* Portrait = Config->Portrait.LoadSynchronous())
			{
				Protrait->SetBrushFromTexture(Portrait, /*bMatchSize=*/true);
			}
		}
	}

	return true;
}

void UAshHUDWidget::BindToCombatFeed()
{
	if (bBoundToFeed)
	{
		return;
	}
	if (UAshCombatFeedSubsystem* Feed = UAshCombatFeedSubsystem::Get(this))
	{
		Feed->OnPlayerKillCountChanged.AddDynamic(this, &UAshHUDWidget::HandleKillCountChanged);
		Feed->OnUnitStruck.AddDynamic(this, &UAshHUDWidget::HandleUnitStruck);
		UpdateKillCountText(Feed->GetPlayerKillCount());
		bBoundToFeed = true;
	}
}

void UAshHUDWidget::RefreshTargetsFromASC()
{
	UAshAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC)
	{
		return;
	}

	const float Health = ASC->GetNumericAttribute(UAshAttributeSet::GetHealthAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UAshAttributeSet::GetMaxHealthAttribute());
	HealthTarget = (MaxHealth > 0.f) ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;

	const float Stamina = ASC->GetNumericAttribute(UAshAttributeSet::GetStaminaAttribute());
	const float MaxStamina = ASC->GetNumericAttribute(UAshAttributeSet::GetMaxStaminaAttribute());
	ManaTarget = (MaxStamina > 0.f) ? FMath::Clamp(Stamina / MaxStamina, 0.f, 1.f) : 0.f;
}

void UAshHUDWidget::HandleAttributeChanged(const FOnAttributeChangeData& /*Data*/)
{
	RefreshTargetsFromASC();
}

void UAshHUDWidget::HandleKillCountChanged(int32 NewKillCount)
{
	UpdateKillCountText(NewKillCount);
	OnKillCountChanged(NewKillCount);
}

void UAshHUDWidget::HandleUnitStruck(const FAshStruckUnitInfo& Info)
{
	if (RecentTargetPanel)
	{
		RecentTargetPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
		RecentTargetPanel->SetTargetInfo(Info);
	}
	OnUnitStruck(Info);
}

void UAshHUDWidget::UpdateKillCountText(int32 Count)
{
	if (KillCountText)
	{
		KillCountText->SetText(FText::Format(KillCountFormat, FText::AsNumber(Count)));
	}
}

void UAshHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Keep retrying the hero bind until the experience-spawned pawn is possessed.
	if (!BoundASC.IsValid())
	{
		TryBindToHero();
	}
	if (!bBoundToFeed)
	{
		BindToCombatFeed();
	}

	// Smoothly glide each bar toward its live target. The front bar is snappy; the optional trail bar
	// lags behind so a damage chunk reads as a lingering chip before catching up.
	HealthDisplay = FMath::FInterpTo(HealthDisplay, HealthTarget, InDeltaTime, BarInterpSpeed);
	HealthTrailDisplay = FMath::FInterpTo(HealthTrailDisplay, HealthTarget, InDeltaTime, TrailInterpSpeed);
	ManaDisplay = FMath::FInterpTo(ManaDisplay, ManaTarget, InDeltaTime, BarInterpSpeed);
	ManaTrailDisplay = FMath::FInterpTo(ManaTrailDisplay, ManaTarget, InDeltaTime, TrailInterpSpeed);

	if (HealthBar)		{ HealthBar->SetPercent(HealthDisplay); }
	if (HealthBarTrail)	{ HealthBarTrail->SetPercent(HealthTrailDisplay); }
	if (ManaBar)		{ ManaBar->SetPercent(ManaDisplay); }
	if (ManaBarTrail)	{ ManaBarTrail->SetPercent(ManaTrailDisplay); }
}
