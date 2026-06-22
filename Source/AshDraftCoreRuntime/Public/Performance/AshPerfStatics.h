// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * AshPerfStatics.h
 *
 * Phase 18 performance-validation switches (PLAN Phase 18, ARCHITECTURE.md 9 / 7.5). The
 * actual measurement happens in PIE (FPS, stat unit/mass), but these console-variable
 * accessors let the Mass processors toggle AI LOD and time slicing at runtime so the
 * "compare AI LOD on/off" and "compare time slicing on/off" tasks need no rebuild — just:
 *
 *     Ash.Mass.LOD 0          (disable LOD: all soldiers updated at full rate)
 *     Ash.Mass.TimeSlice 0    (disable time slicing: whole population re-evaluated per frame)
 *
 * Companion console commands (defined in AshPerfStatics.cpp), all world-scoped:
 *     Ash.Perf.SpawnSoldiers <count>   set every Mass spawner to <count> and respawn
 *     Ash.Perf.Report                  log FPS + soldier/base/proxy counts + toggle state
 *
 * The processors call these accessors instead of reading the CVars directly so the default
 * (enabled) is centralised and the CVar names live in one place.
 */
namespace AshPerf
{
	/** False when `Ash.Mass.LOD 0`: processors should treat every soldier as full-fidelity. */
	ASHDRAFTCORERUNTIME_API bool IsLODEnabled();

	/** False when `Ash.Mass.TimeSlice 0`: the LOD processor recomputes all soldiers each frame. */
	ASHDRAFTCORERUNTIME_API bool IsTimeSlicingEnabled();
}
