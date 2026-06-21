// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AshAnimNotifyState_ComboWindow.generated.h"

/**
 * UAshAnimNotifyState_ComboWindow
 *
 * Drop this notify state on an attack montage to mark the span during which a
 * repeat attack press cancels the current swing and chains the next combo hit.
 * NotifyBegin sends Ash.Event.Combo.WindowOpen and NotifyEnd sends
 * Ash.Event.Combo.WindowClose to the animating actor; the active attack ability
 * listens for both.
 *
 * Place it from roughly the contact frame to the end of the montage so the
 * recovery tail becomes cancellable (this is what removes the slow "after delay"
 * between hits). A press during the wind-up before the window is buffered and
 * fires the instant the window opens.
 */
UCLASS(meta = (DisplayName = "Ash Combo Window"))
class ASHDRAFTCORERUNTIME_API UAshAnimNotifyState_ComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAshAnimNotifyState_ComboWindow();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** Event sent when the window opens (default Ash.Event.Combo.WindowOpen). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash", meta = (Categories = "Ash.Event"))
	FGameplayTag OpenEventTag;

	/** Event sent when the window closes (default Ash.Event.Combo.WindowClose). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash", meta = (Categories = "Ash.Event"))
	FGameplayTag CloseEventTag;
};
