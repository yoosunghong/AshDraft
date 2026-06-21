// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Teams/AshTeamTypes.h"
#include "AshBaseSubsystem.generated.h"

class AAshBaseActor;

/**
 * Global base-ownership notification (ARCHITECTURE.md 8.1 / 11.2 / 16).
 * Commander AI subscribes here once instead of binding to every base actor.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAshOnAnyBaseOwnershipChanged, AAshBaseActor*, Base, EAshTeamId, OldTeam, EAshTeamId, NewTeam);

/**
 * UAshBaseSubsystem
 *
 * World-scoped registry and event hub for battlefield bases. Bases register on
 * BeginPlay and unregister on EndPlay; when any base changes owner it routes the
 * event through OnAnyBaseOwnershipChanged so the commander AI and UI can react
 * without a hard dependency on individual base actors (ARCHITECTURE.md 18.4).
 *
 * Also answers cheap strategic queries (base counts per team) the commander needs
 * to decide attack/defense objectives.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshBaseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Adds a base to the registry (idempotent). Called by the base on BeginPlay. */
	void RegisterBase(AAshBaseActor* Base);

	/** Removes a base from the registry. Called by the base on EndPlay. */
	void UnregisterBase(AAshBaseActor* Base);

	/** Called by a base after its owner changes; re-broadcasts to global listeners. */
	void NotifyOwnershipChanged(AAshBaseActor* Base, EAshTeamId OldTeam, EAshTeamId NewTeam);

	/** Number of currently-registered bases owned by Team. */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	int32 GetBaseCountForTeam(EAshTeamId Team) const;

	/** All registered bases (compacted of stale entries). */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	TArray<AAshBaseActor*> GetAllBases() const;

	/** Fired whenever any registered base changes ownership. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Base")
	FAshOnAnyBaseOwnershipChanged OnAnyBaseOwnershipChanged;

private:
	/** Registered bases. Weak so a destroyed base does not linger. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AAshBaseActor>> Bases;
};
