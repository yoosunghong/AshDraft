// Copyright YoosungHong. All Rights Reserved.

#include "Performance/AshPerfStatics.h"

#include "Base/AshBaseSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Mass/AshMassSoldierSpawner.h"
#include "Mass/AshSoldierProxyPool.h"
#include "Soldier/AshSoldierSubsystem.h"
#include "Teams/AshTeamTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshPerf, Log, All);

// --- Toggles (consumed by the Mass LOD processor) --------------------------------------

static TAutoConsoleVariable<int32> CVarAshMassLOD(
	TEXT("Ash.Mass.LOD"),
	1,
	TEXT("AshDraft AI LOD. 1 = enabled (default). 0 = disabled: every soldier is treated as LOD 0\n")
	TEXT("(full update rate), the baseline for the Phase 18 'compare AI LOD on/off' measurement."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAshMassTimeSlice(
	TEXT("Ash.Mass.TimeSlice"),
	1,
	TEXT("AshDraft LOD time slicing. 1 = enabled (default): 1/N of soldiers re-evaluated per frame.\n")
	TEXT("0 = disabled: the whole population is re-evaluated every frame (Phase 18 'time slicing off')."),
	ECVF_Default);

bool AshPerf::IsLODEnabled()
{
	return CVarAshMassLOD.GetValueOnAnyThread() != 0;
}

bool AshPerf::IsTimeSlicingEnabled()
{
	return CVarAshMassTimeSlice.GetValueOnAnyThread() != 0;
}

// --- Console commands ------------------------------------------------------------------

namespace
{
	void AshPerfSpawnSoldiers(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		const int32 Count = (Args.Num() > 0) ? FMath::Max(0, FCString::Atoi(*Args[0])) : 300;

		int32 TotalSpawned = 0;
		int32 SpawnerCount = 0;
		for (TActorIterator<AAshMassSoldierSpawner> It(World); It; ++It)
		{
			It->SetDesiredSpawnCount(Count);
			TotalSpawned += It->Respawn();
			++SpawnerCount;
		}

		UE_LOG(LogAshPerf, Log, TEXT("Ash.Perf.SpawnSoldiers: set %d spawner(s) to %d each; total live = %d."),
			SpawnerCount, Count, TotalSpawned);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
				FString::Printf(TEXT("SpawnSoldiers: %d spawners x %d = %d live"), SpawnerCount, Count, TotalSpawned));
		}
	}

	void AshPerfReport(const TArray<FString>& /*Args*/, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		// Quick instantaneous FPS from the last frame. `stat unit` / `stat mass` remain the
		// authoritative source for game-thread and Mass-processor cost (see the Phase 18 guide).
		const float Delta = World->GetDeltaSeconds();
		const float FPS = (Delta > 0.f) ? (1.f / Delta) : 0.f;
		const float FrameMs = (Delta > 0.f) ? (Delta * 1000.f) : 0.f;

		// Mass soldier population across all spawners.
		int32 MassSoldiers = 0;
		int32 SpawnerCount = 0;
		for (TActorIterator<AAshMassSoldierSpawner> It(World); It; ++It)
		{
			MassSoldiers += It->GetSpawnedCount();
			++SpawnerCount;
		}

		int32 ActorSoldiers = 0;
		if (const UAshSoldierSubsystem* SoldierSubsystem = World->GetSubsystem<UAshSoldierSubsystem>())
		{
			ActorSoldiers = SoldierSubsystem->GetAllSoldiers().Num();
		}

		int32 ActiveProxies = 0;
		if (const UAshSoldierProxyPool* ProxyPool = World->GetSubsystem<UAshSoldierProxyPool>())
		{
			ActiveProxies = ProxyPool->GetActiveProxyCount();
		}

		int32 PlayerBases = 0, AllyBases = 0, EnemyBases = 0, NeutralBases = 0;
		if (const UAshBaseSubsystem* BaseSubsystem = World->GetSubsystem<UAshBaseSubsystem>())
		{
			PlayerBases = BaseSubsystem->GetBaseCountForTeam(EAshTeamId::Player);
			AllyBases = BaseSubsystem->GetBaseCountForTeam(EAshTeamId::Ally);
			EnemyBases = BaseSubsystem->GetBaseCountForTeam(EAshTeamId::Enemy);
			NeutralBases = BaseSubsystem->GetBaseCountForTeam(EAshTeamId::Neutral);
		}

		const bool bLOD = AshPerf::IsLODEnabled();
		const bool bTimeSlice = AshPerf::IsTimeSlicingEnabled();

		UE_LOG(LogAshPerf, Log,
			TEXT("Ash.Perf.Report | FPS=%.1f (%.2f ms) | MassSoldiers=%d (%d spawners) | ActorSoldiers=%d | ActiveProxies=%d | Bases P/A/E/N=%d/%d/%d/%d | LOD=%s TimeSlice=%s"),
			FPS, FrameMs, MassSoldiers, SpawnerCount, ActorSoldiers, ActiveProxies,
			PlayerBases, AllyBases, EnemyBases, NeutralBases,
			bLOD ? TEXT("ON") : TEXT("OFF"), bTimeSlice ? TEXT("ON") : TEXT("OFF"));

		if (GEngine)
		{
			const FString Msg = FString::Printf(
				TEXT("ASH PERF\nFPS %.1f (%.2f ms)\nMass soldiers %d (%d spawners)\nActor soldiers %d  proxies %d\nBases P/A/E/N %d/%d/%d/%d\nLOD %s  TimeSlice %s"),
				FPS, FrameMs, MassSoldiers, SpawnerCount, ActorSoldiers, ActiveProxies,
				PlayerBases, AllyBases, EnemyBases, NeutralBases,
				bLOD ? TEXT("ON") : TEXT("OFF"), bTimeSlice ? TEXT("ON") : TEXT("OFF"));
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow, Msg);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GAshPerfSpawnSoldiersCmd(
	TEXT("Ash.Perf.SpawnSoldiers"),
	TEXT("Set every AshMassSoldierSpawner to <count> and respawn. Usage: Ash.Perf.SpawnSoldiers 1000"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AshPerfSpawnSoldiers));

static FAutoConsoleCommandWithWorldAndArgs GAshPerfReportCmd(
	TEXT("Ash.Perf.Report"),
	TEXT("Log a one-shot perf snapshot: FPS, Mass/actor soldier counts, active proxies, base counts, LOD/time-slice state."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AshPerfReport));
