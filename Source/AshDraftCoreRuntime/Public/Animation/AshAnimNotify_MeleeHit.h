// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AshAnimNotify_MeleeHit.generated.h"

/**
 * UAshAnimNotify_MeleeHit
 *
 * Drop this notify on an attack montage at the contact frame. When the montage
 * reaches it, it sends a gameplay event (default Ash.Event.Hit.Melee) to the
 * animating actor. The active attack ability listens for that event and runs its
 * damage sweep at that exact moment — so damage timing is authored in the
 * animation, not hardcoded.
 */
UCLASS(meta = (DisplayName = "Ash Melee Hit"))
class ASHDRAFTCORERUNTIME_API UAshAnimNotify_MeleeHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAshAnimNotify_MeleeHit();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** Gameplay event tag sent to the mesh's owning actor at this frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash", meta = (Categories = "Ash.Event"))
	FGameplayTag EventTag;
};
