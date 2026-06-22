// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Teams/AshTeamTypes.h"
#include "AshSoldierSubsystem.generated.h"

class AAshSoldierCharacter;

/**
 * UAshSoldierSubsystem
 *
 * World-scoped registry for the Phase 8 prototype soldiers (ARCHITECTURE.md 18.4:
 * prefer subsystem queries over hard references between units). Soldiers register on
 * BeginPlay and unregister on death/EndPlay; each soldier's think loop asks the
 * subsystem for its nearest hostile target instead of running its own world sweep.
 *
 * This is the seam where the hierarchical AI (Phase 11) and the Mass migration
 * (Phase 9) plug in: today it answers "nearest enemy" and "how many soldiers per
 * team"; later it can hand those queries to a spatial grid / Mass entity query
 * without changing the soldier actor.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshSoldierSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Adds a soldier to the registry (idempotent). Called by the soldier on BeginPlay. */
	void RegisterSoldier(AAshSoldierCharacter* Soldier);

	/** Removes a soldier from the registry. Called on death and EndPlay. */
	void UnregisterSoldier(AAshSoldierCharacter* Soldier);

	/**
	 * Returns the closest living soldier hostile to SeekerTeam within MaxDistance of
	 * FromLocation, or nullptr. Hostility is resolved through UAshTeamStatics so it
	 * matches the rest of the project's team rules.
	 */
	AAshSoldierCharacter* FindNearestHostileSoldier(EAshTeamId SeekerTeam, const FVector& FromLocation, float MaxDistance) const;

	/** Number of currently-registered living soldiers on Team. */
	UFUNCTION(BlueprintPure, Category = "Ash|Soldier")
	int32 GetSoldierCountForTeam(EAshTeamId Team) const;

	/** All registered living soldiers (stale/dead entries skipped). */
	UFUNCTION(BlueprintPure, Category = "Ash|Soldier")
	TArray<AAshSoldierCharacter*> GetAllSoldiers() const;

private:
	/** Registered soldiers. Weak so a destroyed soldier does not linger. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AAshSoldierCharacter>> Soldiers;
};
