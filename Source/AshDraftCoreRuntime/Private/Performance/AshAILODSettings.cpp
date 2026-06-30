// Copyright YoosungHong. All Rights Reserved.

#include "Performance/AshAILODSettings.h"

int32 UAshAILODSettings::ComputeLODLevel(float Distance) const
{
	if (Distance <= LOD0MaxDistance) { return 0; }
	if (Distance <= LOD1MaxDistance) { return 1; }
	if (Distance <= LOD2MaxDistance) { return 2; }
	return 3;
}

float UAshAILODSettings::GetUpdateInterval(int32 LODLevel) const
{
	const int32 ClampedLevel = FMath::Clamp(LODLevel, 0, 3);
	if (UpdateIntervals.IsValidIndex(ClampedLevel))
	{
		return UpdateIntervals[ClampedLevel];
	}

	static const float DefaultIntervals[4] = { 0.05f, 0.15f, 0.5f, 1.0f };
	return DefaultIntervals[ClampedLevel];
}
