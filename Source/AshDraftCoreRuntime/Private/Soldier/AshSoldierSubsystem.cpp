// Copyright YoosungHong. All Rights Reserved.

#include "Soldier/AshSoldierSubsystem.h"

#include "Soldier/AshSoldierCharacter.h"
#include "Teams/AshTeamStatics.h"

void UAshSoldierSubsystem::RegisterSoldier(AAshSoldierCharacter* Soldier)
{
	if (!Soldier)
	{
		return;
	}

	Soldiers.AddUnique(Soldier);
}

void UAshSoldierSubsystem::UnregisterSoldier(AAshSoldierCharacter* Soldier)
{
	Soldiers.RemoveAll([Soldier](const TWeakObjectPtr<AAshSoldierCharacter>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Soldier;
	});
}

AAshSoldierCharacter* UAshSoldierSubsystem::FindNearestHostileSoldier(EAshTeamId SeekerTeam, const FVector& FromLocation, float MaxDistance) const
{
	AAshSoldierCharacter* Best = nullptr;
	float BestDistSq = MaxDistance > 0.f ? (MaxDistance * MaxDistance) : TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<AAshSoldierCharacter>& Entry : Soldiers)
	{
		AAshSoldierCharacter* Soldier = Entry.Get();
		if (!Soldier || Soldier->IsDead())
		{
			continue;
		}

		if (!UAshTeamStatics::AreTeamsHostile(SeekerTeam, Soldier->GetTeamId()))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(FromLocation, Soldier->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Soldier;
		}
	}

	return Best;
}

int32 UAshSoldierSubsystem::GetSoldierCountForTeam(EAshTeamId Team) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AAshSoldierCharacter>& Entry : Soldiers)
	{
		if (Entry.IsValid() && !Entry->IsDead() && Entry->GetTeamId() == Team)
		{
			++Count;
		}
	}
	return Count;
}

TArray<AAshSoldierCharacter*> UAshSoldierSubsystem::GetAllSoldiers() const
{
	TArray<AAshSoldierCharacter*> Result;
	Result.Reserve(Soldiers.Num());
	for (const TWeakObjectPtr<AAshSoldierCharacter>& Entry : Soldiers)
	{
		if (Entry.IsValid() && !Entry->IsDead())
		{
			Result.Add(Entry.Get());
		}
	}
	return Result;
}
