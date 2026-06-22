// Copyright YoosungHong. All Rights Reserved.

#include "QA/AshTelemetrySubsystem.h"

#include "AbilitySystem/AshAbilitySystemComponent.h"
#include "AshGameplayTags.h"
#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Character/AshEnemyGeneralCharacter.h"
#include "Character/AshHeroCharacter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "JsonObjectConverter.h"
#include "Match/AshMatchSubsystem.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Soldier/AshSoldierCharacter.h"
#include "Soldier/AshSoldierSubsystem.h"
#include "Teams/AshTeamStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshTelemetry, Log, All);

UAshTelemetrySubsystem* UAshTelemetrySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UAshTelemetrySubsystem>() : nullptr;
}

void UAshTelemetrySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UAshBaseSubsystem* BaseSubsystem = InWorld.GetSubsystem<UAshBaseSubsystem>())
	{
		BaseSubsystem->OnAnyBaseOwnershipChanged.AddDynamic(this, &UAshTelemetrySubsystem::HandleBaseOwnershipChanged);
		bBoundToBases = true;
	}

	if (UAshMatchSubsystem* MatchSubsystem = InWorld.GetSubsystem<UAshMatchSubsystem>())
	{
		MatchSubsystem->OnMatchEnded.AddDynamic(this, &UAshTelemetrySubsystem::HandleMatchEnded);
		bBoundToMatch = true;
	}
}

void UAshTelemetrySubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		if (bBoundToBases)
		{
			if (UAshBaseSubsystem* BaseSubsystem = World->GetSubsystem<UAshBaseSubsystem>())
			{
				BaseSubsystem->OnAnyBaseOwnershipChanged.RemoveDynamic(this, &UAshTelemetrySubsystem::HandleBaseOwnershipChanged);
			}
		}
		if (bBoundToMatch)
		{
			if (UAshMatchSubsystem* MatchSubsystem = World->GetSubsystem<UAshMatchSubsystem>())
			{
				MatchSubsystem->OnMatchEnded.RemoveDynamic(this, &UAshTelemetrySubsystem::HandleMatchEnded);
			}
		}
	}
	bBoundToBases = false;
	bBoundToMatch = false;

	Super::Deinitialize();
}

float UAshTelemetrySubsystem::NowSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

AAshHeroCharacter* UAshTelemetrySubsystem::GetPlayerHero() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		return Cast<AAshHeroCharacter>(PC->GetPawn());
	}
	return nullptr;
}

// --- Observation -----------------------------------------------------------------------

FAshObservation UAshTelemetrySubsystem::BuildObservation() const
{
	FAshObservation Obs;
	Obs.TimeSeconds = NowSeconds();

	const UWorld* World = GetWorld();

	// Player vitals + pose.
	EAshTeamId PlayerTeam = EAshTeamId::Player;
	if (const AAshHeroCharacter* Hero = GetPlayerHero())
	{
		Obs.PlayerHealth = Hero->GetHealth();
		Obs.PlayerMaxHealth = Hero->GetMaxHealth();
		Obs.bPlayerAlive = !Hero->IsDead();
		Obs.PlayerPosition = Hero->GetActorLocation();
		PlayerTeam = Hero->GetTeamId();
		if (const AController* C = Hero->GetController())
		{
			Obs.PlayerYaw = C->GetControlRotation().Yaw;
		}
		else
		{
			Obs.PlayerYaw = Hero->GetActorRotation().Yaw;
		}
	}

	Obs.NearbyEnemyCount = CountNearbyEnemies(Obs.PlayerPosition);

	// Strategic base picture.
	if (UAshBaseSubsystem* BaseSubsystem = World ? World->GetSubsystem<UAshBaseSubsystem>() : nullptr)
	{
		Obs.OwnedBases = BaseSubsystem->GetBaseCountForTeam(EAshTeamId::Player)
			+ BaseSubsystem->GetBaseCountForTeam(EAshTeamId::Ally);
		Obs.EnemyBases = BaseSubsystem->GetBaseCountForTeam(EAshTeamId::Enemy);
		Obs.NeutralBases = BaseSubsystem->GetBaseCountForTeam(EAshTeamId::Neutral);
	}

	Obs.CurrentObjective = ComputeCurrentObjective(Obs.PlayerPosition);

	if (UAshMatchSubsystem* MatchSubsystem = World ? World->GetSubsystem<UAshMatchSubsystem>() : nullptr)
	{
		Obs.MatchState = MatchSubsystem->GetMatchState();
	}

	return Obs;
}

int32 UAshTelemetrySubsystem::CountNearbyEnemies(const FVector& FromLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	const float RadiusSq = ObservationRadius * ObservationRadius;
	int32 Count = 0;

	// Actor soldiers (the registry returns only living ones).
	if (const UAshSoldierSubsystem* SoldierSubsystem = World->GetSubsystem<UAshSoldierSubsystem>())
	{
		for (const AAshSoldierCharacter* Soldier : SoldierSubsystem->GetAllSoldiers())
		{
			if (!Soldier)
			{
				continue;
			}
			if (UAshTeamStatics::GetActorTeam(Soldier) != EAshTeamId::Enemy)
			{
				continue;
			}
			if (FVector::DistSquared(FromLocation, Soldier->GetActorLocation()) <= RadiusSq)
			{
				++Count;
			}
		}
	}

	// Enemy generals.
	for (TActorIterator<AAshEnemyGeneralCharacter> It(World); It; ++It)
	{
		const AAshEnemyGeneralCharacter* General = *It;
		if (!General || General->IsDead())
		{
			continue;
		}
		if (FVector::DistSquared(FromLocation, General->GetActorLocation()) <= RadiusSq)
		{
			++Count;
		}
	}

	// NOTE: Mass-entity soldiers are not counted here yet — the actor-registry seam matches the
	// rest of the project (UAshSoldierSubsystem). A Mass spatial query is the future extension.
	return Count;
}

FString UAshTelemetrySubsystem::ComputeCurrentObjective(const FVector& FromLocation) const
{
	const UWorld* World = GetWorld();
	UAshBaseSubsystem* BaseSubsystem = World ? World->GetSubsystem<UAshBaseSubsystem>() : nullptr;
	if (!BaseSubsystem)
	{
		return TEXT("None");
	}

	const auto IsPlayerSide = [](EAshTeamId Team)
	{
		return Team == EAshTeamId::Player || Team == EAshTeamId::Ally;
	};

	AAshBaseActor* NearestCapturable = nullptr;
	AAshBaseActor* NearestOwned = nullptr;
	float BestCapturableDistSq = TNumericLimits<float>::Max();
	float BestOwnedDistSq = TNumericLimits<float>::Max();

	for (AAshBaseActor* Base : BaseSubsystem->GetAllBases())
	{
		if (!Base)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(FromLocation, Base->GetActorLocation());
		if (IsPlayerSide(Base->GetOwningTeam()))
		{
			if (DistSq < BestOwnedDistSq)
			{
				BestOwnedDistSq = DistSq;
				NearestOwned = Base;
			}
		}
		else if (DistSq < BestCapturableDistSq)
		{
			BestCapturableDistSq = DistSq;
			NearestCapturable = Base;
		}
	}

	if (NearestCapturable)
	{
		return FString::Printf(TEXT("Capture:%s"), *NearestCapturable->GetName());
	}
	if (NearestOwned)
	{
		return FString::Printf(TEXT("Defend:%s"), *NearestOwned->GetName());
	}
	return TEXT("None");
}

// --- Action ----------------------------------------------------------------------------

bool UAshTelemetrySubsystem::ApplyAction(const FAshAction& Action)
{
	AAshHeroCharacter* Hero = GetPlayerHero();
	if (!Hero || Hero->IsDead())
	{
		return false;
	}

	AController* Controller = Hero->GetController();

	// Camera yaw: apply the requested delta (degrees) precisely so an agent can steer exactly.
	if (Controller && !FMath::IsNearlyZero(Action.CameraYaw))
	{
		FRotator ControlRot = Controller->GetControlRotation();
		ControlRot.Yaw += Action.CameraYaw;
		Controller->SetControlRotation(ControlRot);
	}

	// Movement: screen-relative, mirroring AAshHeroCharacter::Input_Move (Y=forward, X=right).
	if (Controller && !Action.Move.IsNearlyZero())
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		Hero->AddMovementInput(Forward, Action.Move.Y);
		Hero->AddMovementInput(Right, Action.Move.X);
	}

	// Abilities: route through the ASC by input tag (the same path Enhanced Input uses).
	if (UAshAbilitySystemComponent* ASC = Cast<UAshAbilitySystemComponent>(Hero->GetAbilitySystemComponent()))
	{
		if (Action.bAttack)
		{
			ASC->AbilityInputTagPressed(AshGameplayTags::InputTag_Attack_Basic);
		}
		if (Action.bSkill1)
		{
			ASC->AbilityInputTagPressed(AshGameplayTags::InputTag_Attack_Heavy);
		}
		if (Action.bDodge)
		{
			ASC->AbilityInputTagPressed(AshGameplayTags::InputTag_Dodge);
		}
	}

	return true;
}

// --- Logging ---------------------------------------------------------------------------

template <typename T>
void UAshTelemetrySubsystem::AppendCapped(TArray<T>& Buffer, const T& Entry)
{
	Buffer.Add(Entry);
	const int32 Cap = FMath::Max(1, MaxLogEntries);
	if (Buffer.Num() > Cap)
	{
		// Drop the oldest overflow in one shift (rare: only when the cap is exceeded).
		Buffer.RemoveAt(0, Buffer.Num() - Cap, EAllowShrinking::No);
	}
}

void UAshTelemetrySubsystem::LogCombatEvent(const FString& Source, const FString& Target, float Amount, FName EventType)
{
	FAshCombatLogEntry Entry;
	Entry.TimeSeconds = NowSeconds();
	Entry.Source = Source;
	Entry.Target = Target;
	Entry.Amount = Amount;
	Entry.EventType = EventType;
	AppendCapped(CombatLog, Entry);
}

void UAshTelemetrySubsystem::HandleBaseOwnershipChanged(AAshBaseActor* Base, EAshTeamId OldTeam, EAshTeamId NewTeam)
{
	FAshBaseEventLogEntry Entry;
	Entry.TimeSeconds = NowSeconds();
	Entry.BaseName = Base ? Base->GetName() : TEXT("<null>");
	Entry.OldTeam = static_cast<int32>(OldTeam);
	Entry.NewTeam = static_cast<int32>(NewTeam);
	AppendCapped(BaseEventLog, Entry);
}

void UAshTelemetrySubsystem::HandleMatchEnded(EAshMatchResult Result, EAshMatchEndReason Reason)
{
	FAshMatchResultLogEntry Entry;
	Entry.TimeSeconds = NowSeconds();
	Entry.Result = Result;
	Entry.Reason = Reason;
	if (const UWorld* World = GetWorld())
	{
		if (const UAshMatchSubsystem* MatchSubsystem = World->GetSubsystem<UAshMatchSubsystem>())
		{
			Entry.MatchDuration = MatchSubsystem->GetMatchElapsedSeconds();
		}
	}
	AppendCapped(MatchResultLog, Entry);

	UE_LOG(LogAshTelemetry, Log, TEXT("Match result logged: Result=%d Reason=%d Duration=%.1fs (combat=%d base=%d)"),
		static_cast<int32>(Result), static_cast<int32>(Reason), Entry.MatchDuration, CombatLog.Num(), BaseEventLog.Num());
}

// --- JSON export -----------------------------------------------------------------------

FString UAshTelemetrySubsystem::GetObservationJson() const
{
	const FAshObservation Obs = BuildObservation();
	FString Out;
	FJsonObjectConverter::UStructToJsonObjectString(Obs, Out);
	return Out;
}

FString UAshTelemetrySubsystem::ExportToJson() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	// Observation.
	const FAshObservation Obs = BuildObservation();
	const TSharedRef<FJsonObject> ObsJson = MakeShared<FJsonObject>();
	FJsonObjectConverter::UStructToJsonObject(FAshObservation::StaticStruct(), &Obs, ObsJson, 0, 0);
	Root->SetObjectField(TEXT("observation"), ObsJson);

	// Helper: convert a struct array into a JSON array field.
	auto AddLogArray = [&Root](const FString& Field, auto& Buffer, UScriptStruct* StructType)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Buffer.Num());
		for (const auto& Entry : Buffer)
		{
			const TSharedRef<FJsonObject> EntryJson = MakeShared<FJsonObject>();
			FJsonObjectConverter::UStructToJsonObject(StructType, &Entry, EntryJson, 0, 0);
			Values.Add(MakeShared<FJsonValueObject>(EntryJson));
		}
		Root->SetArrayField(Field, Values);
	};

	AddLogArray(TEXT("combat_log"), CombatLog, FAshCombatLogEntry::StaticStruct());
	AddLogArray(TEXT("base_event_log"), BaseEventLog, FAshBaseEventLogEntry::StaticStruct());
	AddLogArray(TEXT("match_result_log"), MatchResultLog, FAshMatchResultLogEntry::StaticStruct());

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

FString UAshTelemetrySubsystem::ExportToFile(const FString& OptionalAbsolutePath)
{
	FString Path = OptionalAbsolutePath;
	if (Path.IsEmpty())
	{
		const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AshTelemetry"));
		const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		Path = FPaths::Combine(Dir, FString::Printf(TEXT("AshTelemetry_%s.json"), *Stamp));
	}

	const FString Json = ExportToJson();
	if (FFileHelper::SaveStringToFile(Json, *Path))
	{
		UE_LOG(LogAshTelemetry, Log, TEXT("Telemetry exported to '%s' (%d bytes)."), *Path, Json.Len());
		return Path;
	}

	UE_LOG(LogAshTelemetry, Warning, TEXT("Failed to write telemetry to '%s'."), *Path);
	return FString();
}
