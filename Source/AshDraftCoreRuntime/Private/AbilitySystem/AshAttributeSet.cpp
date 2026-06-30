// Copyright YoosungHong. All Rights Reserved.

#include "AbilitySystem/AshAttributeSet.h"

#include "AshGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "QA/AshTelemetrySubsystem.h"
#include "Teams/AshTeamAgentInterface.h"
#include "UI/AshCombatFeedSubsystem.h"

UAshAttributeSet::UAshAttributeSet()
{
	// Sensible PoC defaults; designers override the base values via the hero's
	// initial-stat properties (ARCHITECTURE.md 17) when the ability system inits.
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitAttackPower(20.f);
	InitDefense(5.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
}

void UAshAttributeSet::ClampToMax(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
}

void UAshAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampToMax(Attribute, NewValue);
}

void UAshAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampToMax(Attribute, NewValue);
}

void UAshAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	UAbilitySystemComponent* TargetASC = &Data.Target;

	// Route the transient Damage meta attribute into a real Health change so all
	// damage flows through one authoritative path (ARCHITECTURE.md 5.5).
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f);

		if (LocalDamage > 0.f)
		{
			const float NewHealth = FMath::Clamp(GetHealth() - LocalDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);

			// Telemetry: record the hit at the one authoritative damage choke point (Phase 17 /
			// ARCHITECTURE.md 14.4). Soft, null-guarded — no dependency if telemetry is absent.
			AActor* TargetActor = Data.Target.GetAvatarActor();
			const FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
			AActor* SourceActor = Context.GetEffectCauser() ? Context.GetEffectCauser() : Context.GetOriginalInstigator();
			if (UAshTelemetrySubsystem* Telemetry = UAshTelemetrySubsystem::Get(TargetActor))
			{
				const FName EventType = (NewHealth <= 0.f) ? FName(TEXT("Kill")) : FName(TEXT("Damage"));
				Telemetry->LogCombatEvent(
					SourceActor ? SourceActor->GetName() : TEXT("<unknown>"),
					TargetActor ? TargetActor->GetName() : TEXT("<unknown>"),
					LocalDamage, EventType);
			}

			// HUD combat feed (Phase 30): record the player's own strikes on ASC-owning units (generals).
			// The feed ignores non-player instigators, so this is safe to call unconditionally.
			if (UAshCombatFeedSubsystem* Feed = UAshCombatFeedSubsystem::Get(TargetActor))
			{
				EAshTeamId VictimTeam = EAshTeamId::Neutral;
				FText VictimName;
				UTexture2D* VictimPortrait = nullptr;
				if (const IAshTeamAgentInterface* Agent = Cast<IAshTeamAgentInterface>(TargetActor))
				{
					VictimTeam = Agent->GetAshTeamId();
					VictimName = Agent->GetAshDisplayName();
					VictimPortrait = Agent->GetAshPortrait();
				}
				const float InMaxHealth = FMath::Max(1.f, GetMaxHealth());
				Feed->ReportPlayerStrike(SourceActor, VictimName, VictimTeam, NewHealth / InMaxHealth, NewHealth <= 0.f, VictimPortrait);
			}
		}
	}

	// Keep Health within bounds for any other effect that touches it directly.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}

	// Death is event-driven via a loose state tag (ARCHITECTURE.md 4.4 / 15):
	// downstream systems (AI, UI, ability cancellation) react to Ash.State.Dead.
	if (TargetASC && (Data.EvaluatedData.Attribute == GetHealthAttribute() || Data.EvaluatedData.Attribute == GetDamageAttribute()))
	{
		const bool bIsDead = GetHealth() <= 0.f;
		if (bIsDead && !TargetASC->HasMatchingGameplayTag(AshGameplayTags::State_Dead))
		{
			TargetASC->AddLooseGameplayTag(AshGameplayTags::State_Dead);
		}
	}
}
