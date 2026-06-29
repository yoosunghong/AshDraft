// Copyright YoosungHong. All Rights Reserved.

#include "Character/AshGeneralCharacter.h"

#include "AbilitySystem/AshAbilitySystemComponent.h"
#include "AbilitySystem/AshAttributeSet.h"
#include "AbilitySystem/AshGameplayAbility.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "AI/AshCommanderSubsystem.h"
#include "AI/AshGeneralConfig.h"
#include "AI/AshGeneralController.h"
#include "AI/AshGeneralSubsystem.h"
#include "AI/AshSquadSubsystem.h"
#include "AshGameplayTags.h"
#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Mass/AshMassSoldierConfig.h"
#include "Mass/AshMassSoldierSpawnLibrary.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Teams/AshTeamStatics.h"

namespace
{
	// Squad ids for general-owned squads start high so they never collide with the small ids placed
	// AAshMassSoldierSpawner actors use (0,1,2,...). Unique within a world is all that's required.
	int32 GAshNextGeneralSquadId = 1000;
}

AAshGeneralCharacter::AAshGeneralCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No per-frame Actor Tick (ARCHITECTURE.md 18.3); behavior is StateTree/GAS/timer/delegate-driven.
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = AAshGeneralController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Face travel direction; movement is NavMesh-driven by the AI (ARCHITECTURE.md 12).
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, 480.f, 0.f);
		MoveComp->bConstrainToPlane = true;
		MoveComp->bSnapToPlaneAtStart = true;
		MoveComp->MaxWalkSpeed = 450.f;
	}

	// GAS: single-player PoC keeps the ASC on the avatar (ARCHITECTURE.md 5), replicated so
	// damage/death stay applicable from an authoritative context (5.5 / 15).
	AbilitySystemComponent = CreateDefaultSubobject<UAshAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAshAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AAshGeneralCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAshGeneralCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void AAshGeneralCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAshAttributeSet::GetHealthAttribute()).AddUObject(this, &AAshGeneralCharacter::OnHealthChanged);
	}

	// Raise the troops and slot into the hierarchical AI (squad + general registries).
	SpawnTroops();

	UWorld* World = GetWorld();
	if (UAshGeneralSubsystem* GeneralSubsystem = World ? World->GetSubsystem<UAshGeneralSubsystem>() : nullptr)
	{
		GeneralId = GeneralSubsystem->RegisterGeneral(this, TeamId, SquadId);
	}

	// A new general changes the strategic picture: let the commander hand it an order now.
	if (UAshCommanderSubsystem* Commander = World ? World->GetSubsystem<UAshCommanderSubsystem>() : nullptr)
	{
		Commander->ReevaluateOrders();
	}
}

void AAshGeneralCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAshAttributeSet::GetHealthAttribute()).RemoveAll(this);
	}

	UWorld* World = GetWorld();

	// Destroy the troop entities this general owns so they don't leak on teardown.
	if (TroopEntities.Num() > 0)
	{
		if (UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr)
		{
			FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
			for (const FMassEntityHandle& Entity : TroopEntities)
			{
				if (EntityManager.IsEntityValid(Entity))
				{
					EntityManager.DestroyEntity(Entity);
				}
			}
		}
		TroopEntities.Reset();
	}

	if (UAshGeneralSubsystem* GeneralSubsystem = World ? World->GetSubsystem<UAshGeneralSubsystem>() : nullptr)
	{
		GeneralSubsystem->UnregisterGeneral(GeneralId);
	}

	Super::EndPlay(EndPlayReason);
}

void AAshGeneralCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		InitializeAttributes();
		GrantDefaultAbilities();
	}
}

void AAshGeneralCharacter::InitializeAttributes()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxHealthAttribute(), InitialMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetHealthAttribute(), InitialMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetAttackPowerAttribute(), InitialAttackPower);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetDefenseAttribute(), InitialDefense);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxStaminaAttribute(), InitialMaxStamina);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetStaminaAttribute(), InitialMaxStamina);
}

void AAshGeneralCharacter::GrantDefaultAbilities()
{
	if (!AbilitySystemComponent || bAbilitiesGranted)
	{
		return;
	}

	for (const TSubclassOf<UAshGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
		if (const UAshGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject())
		{
			const FGameplayTag& AbilityInputTag = AbilityCDO->GetInputTag();
			if (AbilityInputTag.IsValid())
			{
				Spec.GetDynamicSpecSourceTags().AddTag(AbilityInputTag);
			}
		}
		AbilitySystemComponent->GiveAbility(Spec);
	}

	bAbilitiesGranted = true;
}

void AAshGeneralCharacter::SpawnTroops()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	SquadId = GAshNextGeneralSquadId++;

	// TroopCount is the number of 5-soldier squads (fireteams); the body is that many V-cells.
	constexpr int32 FireteamSize = 5;
	const int32 SquadCount = Config ? Config->TroopCount : 0;

	AshMassSoldierSpawn::FAshSoldierSpawnParams Params;
	Params.Config = Config ? Config->SoldierConfig : nullptr;
	Params.TeamId = TeamId;
	Params.SquadId = SquadId;
	Params.FireteamSize = FireteamSize;
	Params.Count = SquadCount * FireteamSize;
	Params.Origin = GetActorLocation();
	Params.Forward = GetActorForwardVector();
	Params.SpawnRadius = Config ? Config->TroopSpawnRadius : 800.f;

	const int32 Num = AshMassSoldierSpawn::SpawnSoldiers(World, Params, TroopEntities);

	if (UAshSquadSubsystem* SquadSubsystem = World->GetSubsystem<UAshSquadSubsystem>())
	{
		SquadSubsystem->RegisterSquad(SquadId, TeamId);
		SquadSubsystem->UpdateSquadAggregate(SquadId, Params.Origin, Num);
	}

	// Seed an initial follow objective so the troops cluster around the general before the StateTree
	// has its first think.
	PublishFollowObjective(EAshSquadOrder::None);
}

void AAshGeneralCharacter::StartOperationalAI()
{
	if (AAshGeneralController* GeneralController = Cast<AAshGeneralController>(GetController()))
	{
		if (UStateTree* StateTree = Config ? Config->StateTree : nullptr)
		{
			GeneralController->RunGeneralStateTree(StateTree);
		}
	}

	// Apply an initial think-rate (LOD 0 until the subsystem's first reclassification pass).
	UpdateThinkLOD(0.f);
}

void AAshGeneralCharacter::SetCommanderOrder(EAshSquadOrder Order, const FVector& ObjectiveLocation)
{
	CommanderOrder = Order;
	CommanderObjective = ObjectiveLocation;
	bHasCommanderObjective = true;
}

void AAshGeneralCharacter::PublishSquadObjective(const FVector& Location, EAshSquadOrder Order)
{
	if (SquadId == INDEX_NONE)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (UAshSquadSubsystem* SquadSubsystem = World ? World->GetSubsystem<UAshSquadSubsystem>() : nullptr)
	{
		const float FormationRadius = Config ? Config->FormationRadius : 700.f;
		// Publish the general's own forward as the stable formation orientation so the troops form up
		// along a fixed direction and settle, instead of orbiting the objective (Phase 27).
		SquadSubsystem->SetSquadObjective(SquadId, Order, Location, FormationRadius, GetActorForwardVector());
	}
}

float AAshGeneralCharacter::GetAttackRange() const
{
	return Config ? Config->AttackRange : FallbackAttackRange;
}

void AAshGeneralCharacter::TriggerBasicAttack()
{
	if (AbilitySystemComponent)
	{
		// Reuse the shared input-driven activation path (same attack ability as the hero / Phase 5 BT).
		AbilitySystemComponent->AbilityInputTagPressed(AshGameplayTags::InputTag_Attack_Basic);
	}
}

void AAshGeneralCharacter::ReceiveSoldierDamage(float Amount, AActor* DamageInstigator)
{
	if (bIsDead || Amount <= 0.f || !AbilitySystemComponent)
	{
		return;
	}

	const float CurrentHealth = AbilitySystemComponent->GetNumericAttribute(UAshAttributeSet::GetHealthAttribute());
	AbilitySystemComponent->SetNumericAttributeBase(
		UAshAttributeSet::GetHealthAttribute(),
		FMath::Max(0.f, CurrentHealth - Amount));
}

bool AAshGeneralCharacter::HasThreat(EAshThreatType ThreatType)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// Refresh the sensing cache no more than once per think interval (and never twice the same frame).
	if (Now - LastSenseTime >= FMath::Max(CurrentThinkInterval, KINDA_SMALL_NUMBER))
	{
		RefreshSensing();
	}

	return (ThreatType == EAshThreatType::Enemy) ? SensedEnemy.IsValid() : SensedStronghold.IsValid();
}

AActor* AAshGeneralCharacter::GetSensedStronghold() const
{
	return SensedStronghold.Get();
}

bool AAshGeneralCharacter::IsPlayerEnemyActor(const AActor* Actor) const
{
	const UWorld* World = GetWorld();
	return Actor && World && Actor == UGameplayStatics::GetPlayerPawn(World, 0);
}

bool AAshGeneralCharacter::TryRetargetSensedEnemy(AActor* ExcludedActor, float SearchRadius)
{
	UWorld* World = GetWorld();
	UAshGeneralSubsystem* GeneralSubsystem = World ? World->GetSubsystem<UAshGeneralSubsystem>() : nullptr;
	if (!GeneralSubsystem || SearchRadius <= 0.f)
	{
		SensedEnemy = nullptr;
		return false;
	}

	const FVector MyLocation = GetActorLocation();
	float BestDistSq = FMath::Square(SearchRadius);
	AActor* BestEnemy = nullptr;

	for (int32 OtherId : GeneralSubsystem->GetAllGeneralIds())
	{
		const FAshGeneralState OtherState = GeneralSubsystem->GetGeneralState(OtherId);
		AAshGeneralCharacter* OtherGeneral = OtherState.General.Get();
		if (!OtherGeneral || OtherGeneral == this || OtherGeneral == ExcludedActor || OtherGeneral->IsDead())
		{
			continue;
		}
		if (!UAshTeamStatics::AreTeamsHostile(TeamId, OtherGeneral->GetTeamId()))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(MyLocation, OtherGeneral->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestEnemy = OtherGeneral;
		}
	}

	SensedEnemy = BestEnemy;
	LastSenseTime = World ? World->GetTimeSeconds() : LastSenseTime;
	return BestEnemy != nullptr;
}

void AAshGeneralCharacter::PublishCombatObjectiveNear(AActor* Enemy)
{
	if (!Enemy)
	{
		PublishSquadObjective(GetActorLocation(), EAshSquadOrder::AttackBase);
		return;
	}

	if (SquadId == INDEX_NONE)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (UAshSquadSubsystem* SquadSubsystem = World ? World->GetSubsystem<UAshSquadSubsystem>() : nullptr)
	{
		const FVector CombatCenter = (GetActorLocation() + Enemy->GetActorLocation()) * 0.5f;
		const float FormationRadius = Config ? Config->CombatFormationRadius : 250.f;
		// Face the formation toward the enemy while engaging.
		SquadSubsystem->SetSquadObjective(SquadId, EAshSquadOrder::AttackBase, CombatCenter, FormationRadius,
			Enemy->GetActorLocation() - GetActorLocation());
	}
}

void AAshGeneralCharacter::RefreshSensing()
{
	SensedEnemy = nullptr;
	SensedStronghold = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LastSenseTime = World->GetTimeSeconds();

	const FVector MyLocation = GetActorLocation();
	const float EnemyRadius = Config ? Config->EnemyEngageRadius : 1500.f;
	const float StrongholdRadius = Config ? Config->StrongholdDetourRadius : 2500.f;

	// --- Hostile actors: the player pawn + registered generals on hostile teams. Soldiers are Mass
	// (no actors); they are handled by the soldier layer, so the general's actor-threat sensing is the
	// few high-fidelity combatants (the hero and rival generals), which is cheap (registry iteration).
	float BestEnemyDistSq = EnemyRadius * EnemyRadius;
	auto ConsiderEnemy = [&](AActor* Other)
	{
		if (!Other || Other == this)
		{
			return;
		}
		if (const AAshGeneralCharacter* OtherGeneral = Cast<AAshGeneralCharacter>(Other))
		{
			if (OtherGeneral->IsDead())
			{
				return;
			}
		}
		if (!UAshTeamStatics::AreTeamsHostile(TeamId, UAshTeamStatics::GetActorTeam(Other)))
		{
			return;
		}
		const float DistSq = FVector::DistSquared(MyLocation, Other->GetActorLocation());
		if (DistSq < BestEnemyDistSq)
		{
			BestEnemyDistSq = DistSq;
			SensedEnemy = Other;
		}
	};

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		ConsiderEnemy(PlayerPawn);
	}
	if (UAshGeneralSubsystem* GeneralSubsystem = World->GetSubsystem<UAshGeneralSubsystem>())
	{
		for (int32 OtherId : GeneralSubsystem->GetAllGeneralIds())
		{
			const FAshGeneralState OtherState = GeneralSubsystem->GetGeneralState(OtherId);
			ConsiderEnemy(OtherState.General.Get());
		}
	}

	// --- Hostile base in the path.
	float BestBaseDistSq = StrongholdRadius * StrongholdRadius;
	if (UAshBaseSubsystem* BaseSubsystem = World->GetSubsystem<UAshBaseSubsystem>())
	{
		for (AAshBaseActor* Base : BaseSubsystem->GetAllBases())
		{
			if (!Base || !UAshTeamStatics::AreTeamsHostile(TeamId, Base->GetOwningTeam()))
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(MyLocation, Base->GetActorLocation());
			if (DistSq < BestBaseDistSq)
			{
				BestBaseDistSq = DistSq;
				SensedStronghold = Base;
			}
		}
	}
}

int32 AAshGeneralCharacter::UpdateThinkLOD(float DistanceToPlayer)
{
	const float L0 = Config ? Config->LOD0MaxDistance : 4000.f;
	const float L1 = Config ? Config->LOD1MaxDistance : 10000.f;
	const float L2 = Config ? Config->LOD2MaxDistance : 20000.f;

	int32 Level = 3;
	if (DistanceToPlayer <= L0)      { Level = 0; }
	else if (DistanceToPlayer <= L1) { Level = 1; }
	else if (DistanceToPlayer <= L2) { Level = 2; }
	CurrentLODLevel = Level;

	float Interval = 0.f;
	if (Config && Config->ThinkIntervals.IsValidIndex(Level))
	{
		Interval = Config->ThinkIntervals[Level];
	}
	else
	{
		static const float DefaultIntervals[4] = { 0.033f, 0.1f, 0.5f, 1.0f };
		Interval = DefaultIntervals[FMath::Clamp(Level, 0, 3)];
	}
	CurrentThinkInterval = Interval;

	if (AAshGeneralController* GeneralController = Cast<AAshGeneralController>(GetController()))
	{
		GeneralController->SetThinkInterval(Interval);
	}

	return Level;
}

void AAshGeneralCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bIsDead)
	{
		return;
	}
	if (Data.NewValue <= 0.f)
	{
		HandleDeath();
	}
}

void AAshGeneralCharacter::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}
	bIsDead = true;

	if (AAshGeneralController* GeneralController = Cast<AAshGeneralController>(GetController()))
	{
		GeneralController->StopGeneralStateTree();
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->AddLooseGameplayTag(AshGameplayTags::State_Dead);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	// Play the death animation over the despawn window (Phase 27). Single-node, non-looping playback on the
	// mesh holds the final (downed) frame instead of blending back to idle; harmless if no anim/mesh is set.
	if (DeathAnim)
	{
		if (USkeletalMeshComponent* DeathMesh = GetMesh())
		{
			DeathMesh->PlayAnimation(DeathAnim, /*bLooping=*/false);
		}
	}

	// Remove from the operational registry so the commander stops planning around a dead general.
	// Troops persist (leaderless, holding their last objective) until level teardown / future rally.
	if (UWorld* World = GetWorld())
	{
		if (UAshGeneralSubsystem* GeneralSubsystem = World->GetSubsystem<UAshGeneralSubsystem>())
		{
			GeneralSubsystem->UnregisterGeneral(GeneralId);
		}
	}

	OnGeneralDied.Broadcast(this);

	if (DeathLifeSpan > 0.f)
	{
		SetLifeSpan(DeathLifeSpan);
	}
}

float AAshGeneralCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AAshGeneralCharacter::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}
