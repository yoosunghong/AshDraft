// Copyright YoosungHong. All Rights Reserved.

#include "AI/AshGeneralSubsystem.h"

#include "Character/AshGeneralCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Performance/AshPerfStatics.h"
#include "TimerManager.h"

void UAshGeneralSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Drive actor AI-LOD from a single repeating timer rather than per-general Tick (18.3). Generals
	// register during their own BeginPlay, so by the time this fires the registry is populated.
	InWorld.GetTimerManager().SetTimer(
		LODTimerHandle, this, &UAshGeneralSubsystem::UpdateGeneralLODs, LODUpdatePeriod, /*bLoop=*/true, 0.f);

	LastPlayerDisplacementUpdateTime = InWorld.GetTimeSeconds();
	InWorld.GetTimerManager().SetTimer(
		PlayerDisplacementTimerHandle, this, &UAshGeneralSubsystem::UpdateGeneralPlayerDisplacement,
		PlayerDisplacementUpdatePeriod, /*bLoop=*/true, 0.f);
}

void UAshGeneralSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LODTimerHandle);
		World->GetTimerManager().ClearTimer(PlayerDisplacementTimerHandle);
	}

	Super::Deinitialize();
}

int32 UAshGeneralSubsystem::RegisterGeneral(AAshGeneralCharacter* General, EAshTeamId Team, int32 SquadId)
{
	const int32 GeneralId = NextGeneralId++;

	FAshGeneralState& State = Generals.Add(GeneralId);
	State.GeneralId = GeneralId;
	State.General = General;
	State.TeamId = Team;
	State.SquadId = SquadId;
	State.Order = EAshSquadOrder::None;
	State.bHasObjective = false;
	State.LODLevel = 0;

	if (General)
	{
		const UWorld* World = GetWorld();
		const APawn* PlayerPawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		const float Distance = (AshPerf::IsLODEnabled() && PlayerPawn)
			? FVector::Dist(General->GetActorLocation(), PlayerPawn->GetActorLocation())
			: 0.f;
		State.LODLevel = General->UpdateThinkLOD(Distance);
	}

	return GeneralId;
}

void UAshGeneralSubsystem::UnregisterGeneral(int32 GeneralId)
{
	Generals.Remove(GeneralId);
}

void UAshGeneralSubsystem::SetGeneralOrder(int32 GeneralId, EAshSquadOrder Order, const FVector& ObjectiveLocation)
{
	if (FAshGeneralState* State = Generals.Find(GeneralId))
	{
		State->Order = Order;
		State->ObjectiveLocation = ObjectiveLocation;
		State->bHasObjective = true;

		// Push the order down to the general actor so its StateTree reads it without a registry lookup.
		if (AAshGeneralCharacter* General = State->General.Get())
		{
			General->SetCommanderOrder(Order, ObjectiveLocation);
		}
	}
}

FAshGeneralState UAshGeneralSubsystem::GetGeneralState(int32 GeneralId) const
{
	if (const FAshGeneralState* State = Generals.Find(GeneralId))
	{
		return *State;
	}
	return FAshGeneralState();
}

TArray<int32> UAshGeneralSubsystem::GetAllGeneralIds() const
{
	TArray<int32> Ids;
	Generals.GetKeys(Ids);
	return Ids;
}

TArray<int32> UAshGeneralSubsystem::GetGeneralIdsForTeam(EAshTeamId Team) const
{
	TArray<int32> Ids;
	for (const TPair<int32, FAshGeneralState>& Pair : Generals)
	{
		if (Pair.Value.TeamId == Team)
		{
			Ids.Add(Pair.Key);
		}
	}
	return Ids;
}

bool UAshGeneralSubsystem::IsSquadCommandedByGeneral(int32 SquadId) const
{
	if (SquadId == INDEX_NONE)
	{
		return false;
	}
	for (const TPair<int32, FAshGeneralState>& Pair : Generals)
	{
		if (Pair.Value.SquadId == SquadId)
		{
			return true;
		}
	}
	return false;
}

void UAshGeneralSubsystem::UpdateGeneralLODs()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Distance is measured to the local player pawn (the high-fidelity focus; ARCHITECTURE.md 9.2).
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const FVector PlayerLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	const bool bHasPlayer = PlayerPawn != nullptr;
	const bool bLODEnabled = AshPerf::IsLODEnabled();

	for (TPair<int32, FAshGeneralState>& Pair : Generals)
	{
		AAshGeneralCharacter* General = Pair.Value.General.Get();
		if (!General)
		{
			continue;
		}

		// LOD disabled or no player (e.g. headless QA) -> full fidelity so sim stays correct.
		const float Distance = bLODEnabled && bHasPlayer
			? FVector::Dist(General->GetActorLocation(), PlayerLocation)
			: 0.f;

		Pair.Value.LODLevel = General->UpdateThinkLOD(Distance);
	}
}

void UAshGeneralSubsystem::UpdateGeneralPlayerDisplacement()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	const float DeltaTime = LastPlayerDisplacementUpdateTime > 0.f
		? FMath::Clamp(Now - LastPlayerDisplacementUpdateTime, 0.f, 0.1f)
		: PlayerDisplacementUpdatePeriod;
	LastPlayerDisplacementUpdateTime = Now;

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		return;
	}

	for (TPair<int32, FAshGeneralState>& Pair : Generals)
	{
		if (AAshGeneralCharacter* General = Pair.Value.General.Get())
		{
			General->UpdatePlayerPathDisplacement(DeltaTime, PlayerPawn);
		}
	}
}
