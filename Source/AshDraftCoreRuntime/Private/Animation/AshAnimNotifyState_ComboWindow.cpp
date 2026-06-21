// Copyright YoosungHong. All Rights Reserved.

#include "Animation/AshAnimNotifyState_ComboWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AshGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAshAnimNotifyState_ComboWindow::UAshAnimNotifyState_ComboWindow()
{
	OpenEventTag = AshGameplayTags::Event_Combo_WindowOpen;
	CloseEventTag = AshGameplayTags::Event_Combo_WindowClose;
}

void UAshAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (Owner && OpenEventTag.IsValid())
	{
		FGameplayEventData Payload;
		Payload.EventTag = OpenEventTag;
		Payload.Instigator = Owner;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, OpenEventTag, Payload);
	}
}

void UAshAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (Owner && CloseEventTag.IsValid())
	{
		FGameplayEventData Payload;
		Payload.EventTag = CloseEventTag;
		Payload.Instigator = Owner;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, CloseEventTag, Payload);
	}
}

FString UAshAnimNotifyState_ComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Ash Combo Window");
}
