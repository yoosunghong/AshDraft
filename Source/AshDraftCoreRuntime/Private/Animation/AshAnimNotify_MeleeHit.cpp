// Copyright YoosungHong. All Rights Reserved.

#include "Animation/AshAnimNotify_MeleeHit.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AshGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAshAnimNotify_MeleeHit::UAshAnimNotify_MeleeHit()
{
	EventTag = AshGameplayTags::Event_Hit_Melee;
}

void UAshAnimNotify_MeleeHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (Owner && EventTag.IsValid())
	{
		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = Owner;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
	}
}

FString UAshAnimNotify_MeleeHit::GetNotifyName_Implementation() const
{
	return TEXT("Ash Melee Hit");
}
