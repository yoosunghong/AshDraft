// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "AshUserWidget.generated.h"

class AAshHeroCharacter;

/**
 * UAshUserWidget
 *
 * The common base for every AshDraft Widget Blueprint (Phase 30, ARCHITECTURE.md 16). It carries no
 * gameplay logic — it only provides the shared plumbing the concrete HUD / unit / target widgets need
 * (locating the local hero, a Blueprint construct hook). Author the actual layout in a WBP child of one
 * of the concrete subclasses (UAshHUDWidget / UAshUnitHealthBarWidget / UAshTargetInfoWidget); this
 * class itself is abstract and is never placed directly.
 *
 * Keeping a single project base means future cross-cutting UI concerns (a shared input mode helper, a
 * common style accessor, localisation scope) attach in one place rather than being copy-pasted across
 * every widget.
 */
UCLASS(Abstract)
class ASHDRAFTCORERUNTIME_API UAshUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The local player's hero pawn, if it exists and is an AAshHeroCharacter (else null). */
	UFUNCTION(BlueprintPure, Category = "Ash|UI")
	AAshHeroCharacter* GetLocalHero() const;
};
