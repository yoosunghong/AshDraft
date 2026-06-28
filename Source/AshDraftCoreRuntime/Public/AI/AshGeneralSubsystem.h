// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "AI/AshGeneralTypes.h"
#include "Engine/TimerHandle.h"
#include "AshGeneralSubsystem.generated.h"

class AAshGeneralCharacter;

/**
 * UAshGeneralSubsystem
 *
 * World-scoped registry of Generals — the operational layer between the strategic commander and the
 * data-oriented soldiers (ARCHITECTURE.md 8 / 18.4). It is to generals what UAshSquadSubsystem is to
 * squads: the commander reads each general's record (FAshGeneralState) and assigns it a strategic
 * order; the general's StateTree executes it and drives its own squad.
 *
 * It also owns the **actor AI-LOD** for generals (ARCHITECTURE.md 9). Generals are Characters, not
 * Mass entities, so their LOD is a think-rate throttle, not a Mass fragment: a single repeating timer
 * (no per-general Actor Tick — CLAUDE.md 18.3) classifies each general by distance to the player and
 * tells it to slow or speed up its StateTree / sensing cadence accordingly.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshGeneralSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~UWorldSubsystem interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~End of UWorldSubsystem interface

	/** Registers a general and returns its new unique id. The general owns SquadId. */
	int32 RegisterGeneral(AAshGeneralCharacter* General, EAshTeamId Team, int32 SquadId);

	/** Removes a general from the registry (called on the general's death / EndPlay). */
	void UnregisterGeneral(int32 GeneralId);

	/** Assigns a strategic order + objective to a general (called by the commander). */
	void SetGeneralOrder(int32 GeneralId, EAshSquadOrder Order, const FVector& ObjectiveLocation);

	/** Copy of a general's record (default-constructed if unknown). */
	UFUNCTION(BlueprintPure, Category = "Ash|General")
	FAshGeneralState GetGeneralState(int32 GeneralId) const;

	/** All registered general ids. */
	UFUNCTION(BlueprintPure, Category = "Ash|General")
	TArray<int32> GetAllGeneralIds() const;

	/** Generals belonging to Team. */
	TArray<int32> GetGeneralIdsForTeam(EAshTeamId Team) const;

	/** True if SquadId is commanded by a registered general (commander skips its direct ordering). */
	bool IsSquadCommandedByGeneral(int32 SquadId) const;

private:
	/** Repeating timer body: reclassify every general's actor LOD from player distance. */
	void UpdateGeneralLODs();

	/** GeneralId -> record. The single source of truth for the operational layer this frame. */
	UPROPERTY()
	TMap<int32, FAshGeneralState> Generals;

	/** Monotonic id allocator so general ids stay unique across (de)registration. */
	int32 NextGeneralId = 0;

	/** Drives UpdateGeneralLODs at a fixed cadence (LOD changes slowly; no need to tick every frame). */
	FTimerHandle LODTimerHandle;

	/** Seconds between actor-LOD reclassification passes. */
	static constexpr float LODUpdatePeriod = 0.5f;
};
