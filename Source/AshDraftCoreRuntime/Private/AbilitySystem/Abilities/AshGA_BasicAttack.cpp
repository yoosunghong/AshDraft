// Copyright YoosungHong. All Rights Reserved.

#include "AbilitySystem/Abilities/AshGA_BasicAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/AshAttributeSet.h"
#include "AbilitySystem/AshGameplayEffect_Damage.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AshGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Mass/AshSoldierProxyActor.h"
#include "Teams/AshTeamStatics.h"

UAshGA_BasicAttack::UAshGA_BasicAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputTag = AshGameplayTags::InputTag_Attack_Basic;
	HitEventTag = AshGameplayTags::Event_Hit_Melee;
	ComboInputEventTag = AshGameplayTags::Event_Combo_Next;
	ComboWindowOpenEventTag = AshGameplayTags::Event_Combo_WindowOpen;
	ComboWindowCloseEventTag = AshGameplayTags::Event_Combo_WindowClose;

	// Tag the avatar while attacking; future hit-reaction / AI can read this.
	ActivationOwnedTags.AddTag(AshGameplayTags::State_Attacking);

	// Default to the C++ damage effect; overridable by a Blueprint child.
	DamageEffect = UAshGameplayEffect_Damage::StaticClass();
}

void UAshGA_BasicAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ComboIndex = 0;
	bComboInputBuffered = false;
	bComboWindowOpen = false;

	// No animations authored yet: fall back to a single instant sweep.
	if (ComboMontages.Num() == 0)
	{
		PerformHitSweep();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Persistent listeners for the whole combo (one of each):
	//  - Hit.Melee        -> damage sweep at each montage's contact frame
	//  - Combo.Next        -> repeat attack press (forwarded by the ASC)
	//  - Combo.WindowOpen  -> cancel window opened (from the montage notify state)
	//  - Combo.WindowClose -> cancel window closed
	UAbilityTask_WaitGameplayEvent* HitTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, HitEventTag, nullptr, false);
	HitTask->EventReceived.AddDynamic(this, &UAshGA_BasicAttack::OnHitEventReceived);
	HitTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* ComboTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboInputEventTag, nullptr, false);
	ComboTask->EventReceived.AddDynamic(this, &UAshGA_BasicAttack::OnComboInputReceived);
	ComboTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* WindowOpenTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboWindowOpenEventTag, nullptr, false);
	WindowOpenTask->EventReceived.AddDynamic(this, &UAshGA_BasicAttack::OnComboWindowOpened);
	WindowOpenTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* WindowCloseTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboWindowCloseEventTag, nullptr, false);
	WindowCloseTask->EventReceived.AddDynamic(this, &UAshGA_BasicAttack::OnComboWindowClosed);
	WindowCloseTask->ReadyForActivation();

	PlayComboMontage(0);
}

void UAshGA_BasicAttack::PlayComboMontage(int32 Index)
{
	if (!ComboMontages.IsValidIndex(Index) || !ComboMontages[Index])
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Cancelling into a new montage interrupts the previous animation; detach the
	// old task first so its OnInterrupted doesn't end the whole ability.
	StopCurrentMontageTask();

	ComboIndex = Index;
	bComboWindowOpen = false;
	bComboInputBuffered = false;

	CurrentMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, ComboMontages[Index], ComboMontagePlayRate);
	CurrentMontageTask->OnCompleted.AddDynamic(this, &UAshGA_BasicAttack::OnComboMontageEnded);
	CurrentMontageTask->OnInterrupted.AddDynamic(this, &UAshGA_BasicAttack::OnComboMontageInterrupted);
	CurrentMontageTask->OnCancelled.AddDynamic(this, &UAshGA_BasicAttack::OnComboMontageInterrupted);
	CurrentMontageTask->ReadyForActivation();
}

void UAshGA_BasicAttack::StopCurrentMontageTask()
{
	if (CurrentMontageTask)
	{
		CurrentMontageTask->OnCompleted.RemoveDynamic(this, &UAshGA_BasicAttack::OnComboMontageEnded);
		CurrentMontageTask->OnInterrupted.RemoveDynamic(this, &UAshGA_BasicAttack::OnComboMontageInterrupted);
		CurrentMontageTask->OnCancelled.RemoveDynamic(this, &UAshGA_BasicAttack::OnComboMontageInterrupted);
		CurrentMontageTask->EndTask();
		CurrentMontageTask = nullptr;
	}
}

void UAshGA_BasicAttack::OnComboInputReceived(FGameplayEventData Payload)
{
	if (bComboWindowOpen)
	{
		// Inside the cancel window: interrupt now and chain the next hit immediately.
		AdvanceCombo();
	}
	else
	{
		// Before the window (wind-up): buffer; it fires the moment the window opens.
		bComboInputBuffered = true;
	}
}

void UAshGA_BasicAttack::OnComboWindowOpened(FGameplayEventData Payload)
{
	bComboWindowOpen = true;

	// A press that arrived during the wind-up cancels into the next hit right now.
	if (bComboInputBuffered)
	{
		AdvanceCombo();
	}
}

void UAshGA_BasicAttack::OnComboWindowClosed(FGameplayEventData Payload)
{
	bComboWindowOpen = false;
}

void UAshGA_BasicAttack::AdvanceCombo()
{
	bComboInputBuffered = false;
	bComboWindowOpen = false;

	const int32 NextIndex = ComboIndex + 1;
	if (!ComboMontages.IsValidIndex(NextIndex))
	{
		// Pressed during the final hit's window: no next step — let it play out.
		return;
	}

	PlayComboMontage(NextIndex);
}

void UAshGA_BasicAttack::OnComboMontageEnded()
{
	// A montage reached its end without being cancelled: the combo is over.
	CurrentMontageTask = nullptr;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAshGA_BasicAttack::OnComboMontageInterrupted()
{
	// Only reached on an external interrupt (our own cancels detach first).
	CurrentMontageTask = nullptr;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAshGA_BasicAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Instanced-per-actor: tidy up + reset so the next activation starts clean.
	StopCurrentMontageTask();
	ComboIndex = 0;
	bComboInputBuffered = false;
	bComboWindowOpen = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAshGA_BasicAttack::OnHitEventReceived(FGameplayEventData Payload)
{
	PerformHitSweep();
}

void UAshGA_BasicAttack::PerformHitSweep()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;

	if (!Avatar || !SourceASC || !World || !DamageEffect)
	{
		return;
	}

	// Sweep a sphere in front of the avatar (instant melee hit).
	const FVector Start = Avatar->GetActorLocation();
	const FVector End = Start + Avatar->GetActorForwardVector() * AttackRange;

	TArray<FHitResult> Hits;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AshBasicAttack), false, Avatar);
	World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(AttackRadius), QueryParams);

	// The ability helper builds a context already tagged with this ability and source.
	const FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
	if (!DamageSpec.IsValid())
	{
		return;
	}

	// For Mass soldiers (no ability system, health lives in a Mass fragment) we can't run the GE
	// pipeline, so reuse the attacker's AttackPower as the flat damage — the same source magnitude
	// UAshDamageExecution consumes — clamped to the execution's MinDamage so a hit always lands.
	const EAshTeamId AttackerTeam = UAshTeamStatics::GetActorTeam(Avatar);
	const float SoldierDamage = FMath::Max(SourceASC->GetNumericAttribute(UAshAttributeSet::GetAttackPowerAttribute()), 1.f);

	// Apply once per distinct hit target. Heroes/generals go through GAS; Mass soldier proxies take
	// direct Mass damage (team-checked so the player never cuts down its own army).
	TSet<UAbilitySystemComponent*> HitTargets;
	TSet<AActor*> HitProxies;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
		{
			continue;
		}

		if (AAshSoldierProxyActor* Proxy = Cast<AAshSoldierProxyActor>(HitActor))
		{
			bool bAlreadyHit = false;
			HitProxies.Add(Proxy, &bAlreadyHit);
			if (!bAlreadyHit && UAshTeamStatics::AreTeamsHostile(AttackerTeam, Proxy->GetRepresentedTeam()))
			{
				Proxy->ReceiveMeleeHit(SoldierDamage, Avatar);
			}
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);
		if (!TargetASC || TargetASC == SourceASC || HitTargets.Contains(TargetASC))
		{
			continue;
		}
		HitTargets.Add(TargetASC);

		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
	}
}
