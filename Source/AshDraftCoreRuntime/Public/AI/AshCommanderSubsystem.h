// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Teams/AshTeamTypes.h"
#include "AshCommanderSubsystem.generated.h"

class AAshBaseActor;
class UAshSquadSubsystem;

/**
 * UAshCommanderSubsystem
 *
 * The top of the hierarchical AI (ARCHITECTURE.md 8.1): the Battle Director / Commander
 * layer. It owns no soldiers and runs no per-unit logic — it reasons over base ownership
 * and hands each squad a strategic order (attack/defend an objective base), which the
 * squad subsystem stores and the Mass movement processor steers toward.
 *
 * It reacts to the world rather than polling: it binds once to
 * UAshBaseSubsystem::OnAnyBaseOwnershipChanged and re-evaluates squad orders whenever a
 * base flips, plus once when the world begins play and whenever a squad registers
 * (ARCHITECTURE.md 8.1 "React to base capture events"). No Actor Tick (18.3).
 *
 * Strategy for the PoC is deliberately simple: each squad attacks the nearest base its
 * team does not own; if its team owns everything, it defends its nearest owned base.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API UAshCommanderSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~UWorldSubsystem interface
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~End of UWorldSubsystem interface

	/**
	 * Recomputes and assigns an order + objective for every registered squad. Safe to call
	 * repeatedly; the spawner calls it after registering a squad and it also runs on every
	 * base-ownership change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ash|Commander")
	void ReevaluateOrders();

private:
	/** Bound to the base subsystem's global ownership-change event. */
	UFUNCTION()
	void HandleBaseOwnershipChanged(AAshBaseActor* Base, EAshTeamId OldTeam, EAshTeamId NewTeam);

	/** Resolves the squad subsystem for this world (may be null very early). */
	UAshSquadSubsystem* GetSquadSubsystem() const;

	/** True once we've bound to the base subsystem delegate. */
	bool bBound = false;
};
