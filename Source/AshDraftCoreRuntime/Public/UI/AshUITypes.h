// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Teams/AshTeamTypes.h"
#include "AshUITypes.generated.h"

class UTexture2D;

/**
 * AshUITypes.h
 *
 * Shared, UI-facing data + presentation helpers (Phase 30, ARCHITECTURE.md 16). The UI only
 * *observes* gameplay; these are read-only view structs/helpers the widgets fill from a subsystem,
 * never a place to put gameplay logic.
 */

/**
 * Snapshot of a unit the player recently struck, for the HUD's "recently struck enemy" panel
 * (name, affiliation, health). Built by UAshCombatFeedSubsystem from the damage choke points and
 * handed to UAshTargetInfoWidget. A plain value type: it never holds a hard reference to the unit
 * (ARCHITECTURE.md 18.4), only the data needed to draw it.
 */
USTRUCT(BlueprintType)
struct FAshStruckUnitInfo
{
	GENERATED_BODY()

	/** Display name of the struck unit (unit-type name for soldiers, character name for generals). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI")
	FText UnitName = FText::GetEmpty();

	/** Affiliation of the struck unit. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI")
	EAshTeamId Team = EAshTeamId::Neutral;

	/** Health as a 0-1 fraction at the moment of the strike. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI")
	float HealthFraction = 0.f;

	/** Optional portrait of the struck unit (from UAshHeroConfig::Portrait). Null for generic soldiers. */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI")
	TObjectPtr<UTexture2D> Portrait = nullptr;

	/** False until a strike has actually been recorded (the panel stays hidden while invalid). */
	UPROPERTY(BlueprintReadOnly, Category = "Ash|UI")
	bool bValid = false;
};

/**
 * Pure presentation helpers shared by the HUD / unit / target widgets, exposed to Blueprint so a
 * WBP can colour or label by team without re-deriving the mapping (single source of truth).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshUIStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Human-readable affiliation label for a team ("Ally", "Enemy", "Player", "Neutral"). */
	UFUNCTION(BlueprintPure, Category = "Ash|UI")
	static FText GetTeamDisplayText(EAshTeamId Team);

	/**
	 * Health-bar fill colour for a team: cyan for the player's side (Player/Ally), red for the
	 * enemy, grey for neutral (PLAN Phase 30 unit-bar requirement). The HUD/unit widgets default
	 * to these; a WBP may still override them with its own EditAnywhere colours.
	 */
	UFUNCTION(BlueprintPure, Category = "Ash|UI")
	static FLinearColor GetTeamHealthColor(EAshTeamId Team);
};
