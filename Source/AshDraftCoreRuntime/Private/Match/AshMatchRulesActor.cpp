// Copyright YoosungHong. All Rights Reserved.

#include "Match/AshMatchRulesActor.h"

AAshMatchRulesActor::AAshMatchRulesActor()
{
	// Pure configuration carrier: the match subsystem reads GetRules(); this actor never ticks
	// (ARCHITECTURE.md 18.3).
	PrimaryActorTick.bCanEverTick = false;
}
