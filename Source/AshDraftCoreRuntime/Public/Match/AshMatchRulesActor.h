// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "AshMatchRulesActor.generated.h"

class UAshMatchRulesConfig;

/**
 * AAshMatchRulesActor
 *
 * A tiny placeable that carries a UAshMatchRulesConfig for one level (PLAN Phase 15). Drop one
 * in the map and assign a DA_MatchRules asset; on BeginPlay the UAshMatchSubsystem discovers it
 * and adopts its conditions. With no rules actor in the level the match falls back to the
 * Phase 16 defaults, so existing maps keep working.
 *
 * It is pure configuration: no Tick, no components, no logic — the subsystem owns the match
 * loop (ARCHITECTURE.md 16 / 18.3/18.4). Kept as an Actor (rather than a setting on the
 * Experience) so designers can place and tweak conditions directly in the level editor.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshMatchRulesActor : public AActor
{
	GENERATED_BODY()

public:
	AAshMatchRulesActor();

	/** The rules this actor supplies to the match subsystem (may be null). */
	const UAshMatchRulesConfig* GetRules() const { return Rules; }

protected:
	/** Win/lose conditions for the level. Assign a DA_MatchRules data asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Match")
	TObjectPtr<UAshMatchRulesConfig> Rules;
};
