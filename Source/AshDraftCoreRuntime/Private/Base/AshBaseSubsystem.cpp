// Copyright YoosungHong. All Rights Reserved.

#include "Base/AshBaseSubsystem.h"

#include "Base/AshBaseActor.h"

void UAshBaseSubsystem::RegisterBase(AAshBaseActor* Base)
{
	if (!Base)
	{
		return;
	}

	Bases.AddUnique(Base);
}

void UAshBaseSubsystem::UnregisterBase(AAshBaseActor* Base)
{
	Bases.RemoveAll([Base](const TWeakObjectPtr<AAshBaseActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Base;
	});
}

void UAshBaseSubsystem::NotifyOwnershipChanged(AAshBaseActor* Base, EAshTeamId OldTeam, EAshTeamId NewTeam)
{
	OnAnyBaseOwnershipChanged.Broadcast(Base, OldTeam, NewTeam);
}

int32 UAshBaseSubsystem::GetBaseCountForTeam(EAshTeamId Team) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AAshBaseActor>& Entry : Bases)
	{
		if (Entry.IsValid() && Entry->GetOwningTeam() == Team)
		{
			++Count;
		}
	}
	return Count;
}

TArray<AAshBaseActor*> UAshBaseSubsystem::GetAllBases() const
{
	TArray<AAshBaseActor*> Result;
	Result.Reserve(Bases.Num());
	for (const TWeakObjectPtr<AAshBaseActor>& Entry : Bases)
	{
		if (Entry.IsValid())
		{
			Result.Add(Entry.Get());
		}
	}
	return Result;
}
