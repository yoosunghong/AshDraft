// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "AshMassFireteamProcessor.generated.h"

/**
 * Fireteam-level AI between General StateTree and individual soldier FSM. Maintains persistent
 * five-soldier V formations, arranges fireteams around squad objectives, and assigns combat targets
 * between opposing fireteams or against hostile generals.
 */
UCLASS(config = Game)
class ASHDRAFTCORERUNTIME_API UAshMassFireteamProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAshMassFireteamProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float FireteamSpacing = 520.f;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float FireteamRowSpacing = 460.f;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float FireteamEngageRadius = 2200.f;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float GeneralActorEngageRadius = 1800.f;

	UPROPERTY(config, EditDefaultsOnly, Category = "Ash|Fireteam", meta = (ClampMin = "0.0"))
	float CombatAnchorDistance = 420.f;

private:
	FMassEntityQuery EntityQuery;
};
