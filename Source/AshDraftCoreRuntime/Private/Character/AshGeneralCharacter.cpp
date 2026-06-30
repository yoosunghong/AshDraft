// Copyright YoosungHong. All Rights Reserved.

#include "Character/AshGeneralCharacter.h"

#include "AbilitySystem/AshAbilitySystemComponent.h"
#include "AbilitySystem/AshAttributeSet.h"
#include "AbilitySystem/AshGameplayAbility.h"
#include "AbilitySystem/AshGameplayEffect_Stun.h"
#include "Animation/AnimSequenceBase.h"
#include "Combat/AshCombatRulesSettings.h"
#include "GameplayEffect.h"
#include "Components/SkeletalMeshComponent.h"
#include "AIController.h"
#include "AI/AshCommanderSubsystem.h"
#include "Components/WidgetComponent.h"
#include "Engine/Texture2D.h"
#include "Hero/AshHeroConfig.h"
#include "UI/AshUnitHealthBarWidget.h"
#include "AI/AshGeneralController.h"
#include "AI/AshGeneralSubsystem.h"
#include "AI/AshSquadSubsystem.h"
#include "AshGameplayTags.h"
#include "Base/AshBaseActor.h"
#include "Base/AshBaseSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/ChildActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Mass/AshMassSoldierConfig.h"
#include "Mass/AshMassSoldierSpawnLibrary.h"
#include "Mass/AshSoldierFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Performance/AshAILODSettings.h"
#include "Teams/AshTeamStatics.h"
#include "TimerManager.h"

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

	// Over-head health bar (screen-space, no tick — driven by the attribute-change delegate in BeginPlay).
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(GetMesh());
	HealthBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 220.f));
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidget->SetDrawSize(FVector2D(120.f, 14.f));
	HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default stun effect (overridable per-Blueprint for weapon/skill-specific stuns).
	StunEffectClass = UAshGameplayEffect_Stun::StaticClass();
}

UAbilitySystemComponent* AAshGeneralCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UTexture2D* AAshGeneralCharacter::GetAshPortrait() const
{
	return HeroConfig ? HeroConfig->Portrait.LoadSynchronous() : nullptr;
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

	// Auto-populate DisplayName from the hero config when not explicitly set.
	if (HeroConfig && DisplayName.IsEmpty())
	{
		DisplayName = HeroConfig->HeroName;
	}

	// Seed the over-head health bar now that attributes are initialised.
	if (HealthBarWidget && HealthBarWidgetClass)
	{
		HealthBarWidget->SetWidgetClass(HealthBarWidgetClass);
		HealthBarWidget->InitWidget();
		if (UAshUnitHealthBarWidget* Bar = Cast<UAshUnitHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
		{
			Bar->SetTeam(TeamId);
			Bar->SetHealth(1.f);
			Bar->SetUnitName(DisplayName);
		}
	}

	// Raise the troops and slot into the hierarchical AI (squad + general registries).
	SpawnTroops();

	UWorld* World = GetWorld();
	PlayerDisplacementBasePosition = GetActorLocation();
	LastPlayerPushTime = World ? World->GetTimeSeconds() : LastPlayerPushTime;

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

	// When this general is the AI-controlled version of a player-selectable hero, HeroConfig
	// supplies the canonical base stats so both character types share one source of truth.
	// No player bonuses are applied — the AI always fights at the archetype's base values.
	// Fall back to the per-Blueprint Initial* floats when HeroConfig is not assigned.
	float MaxHealth   = InitialMaxHealth;
	float AttackPower = InitialAttackPower;
	float Defense     = InitialDefense;
	float MaxStamina  = InitialMaxStamina;

	if (HeroConfig)
	{
		MaxHealth   = FMath::Max(1.f, HeroConfig->BaseMaxHealth);
		AttackPower = FMath::Max(0.f, HeroConfig->BaseAttackPower);
		Defense     = FMath::Max(0.f, HeroConfig->BaseDefense);
		MaxStamina  = FMath::Max(1.f, HeroConfig->BaseMaxStamina);
	}

	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxHealthAttribute(),  MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetHealthAttribute(),     MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetAttackPowerAttribute(), AttackPower);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetDefenseAttribute(),    Defense);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetMaxStaminaAttribute(), MaxStamina);
	AbilitySystemComponent->SetNumericAttributeBase(UAshAttributeSet::GetStaminaAttribute(),    MaxStamina);
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

	// Seed morale from the config; it drives the troops' combo chances (Phase 29).
	if (HeroConfig)
	{
		MoraleLevel = FMath::Clamp(HeroConfig->InitialMoraleLevel, 1, FMath::Max(1, HeroConfig->MaxMoraleLevel));
	}

	// TroopCount is the number of 5-soldier squads (fireteams); the body is that many V-cells.
	constexpr int32 FireteamSize = 5;
	const int32 SquadCount = HeroConfig ? HeroConfig->TroopCount : 0;

	AshMassSoldierSpawn::FAshSoldierSpawnParams Params;
	Params.Config = HeroConfig ? HeroConfig->SoldierConfig : nullptr;
	// Stamp the morale-derived combo chances onto every soldier as it spawns.
	ComputeComboChances(Params.TwoHitChance, Params.ThreeHitChance);
	Params.TeamId = TeamId;
	Params.SquadId = SquadId;
	Params.FireteamSize = FireteamSize;
	Params.Count = SquadCount * FireteamSize;
	Params.Origin = GetActorLocation();
	Params.Forward = GetActorForwardVector();
	Params.SpawnRadius = HeroConfig ? HeroConfig->TroopSpawnRadius : 800.f;

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

void AAshGeneralCharacter::ComputeComboChances(float& OutTwoHitChance, float& OutThreeHitChance) const
{
	// Combo chance scales linearly with morale: chance = max-at-full-morale * (level / maxLevel). At the
	// defaults a level-5 general yields 20% (2-hit) / 10% (3-hit); a level-3 general yields 12% / 6%.
	const int32 MaxLevel = HeroConfig ? FMath::Max(1, HeroConfig->MaxMoraleLevel) : 5;
	const float MaxTwo = HeroConfig ? HeroConfig->MaxTwoHitComboChance : 0.20f;
	const float MaxThree = HeroConfig ? HeroConfig->MaxThreeHitComboChance : 0.10f;
	const float Frac = FMath::Clamp(static_cast<float>(MoraleLevel) / static_cast<float>(MaxLevel), 0.f, 1.f);
	OutTwoHitChance = MaxTwo * Frac;
	OutThreeHitChance = MaxThree * Frac;
}

void AAshGeneralCharacter::SetMoraleLevel(int32 NewLevel)
{
	const int32 MaxLevel = HeroConfig ? FMath::Max(1, HeroConfig->MaxMoraleLevel) : 5;
	const int32 Clamped = FMath::Clamp(NewLevel, 1, MaxLevel);
	if (Clamped == MoraleLevel)
	{
		return;
	}
	MoraleLevel = Clamped;

	// Re-stamp every living troop's combo chances so they fight to the new morale immediately (Phase 29).
	float TwoHitChance = 0.f;
	float ThreeHitChance = 0.f;
	ComputeComboChances(TwoHitChance, ThreeHitChance);

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	for (const FMassEntityHandle& Entity : TroopEntities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}
		if (FAshCombatFragment* Combat = EntityManager.GetFragmentDataPtr<FAshCombatFragment>(Entity))
		{
			Combat->TwoHitChance = TwoHitChance;
			Combat->ThreeHitChance = ThreeHitChance;
		}
	}
}

void AAshGeneralCharacter::StartOperationalAI()
{
	if (AAshGeneralController* GeneralController = Cast<AAshGeneralController>(GetController()))
	{
		if (UStateTree* StateTree = HeroConfig ? HeroConfig->StateTree : nullptr)
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
		const float FormationRadius = HeroConfig ? HeroConfig->FormationRadius : 700.f;
		// Publish the general's own forward as the stable formation orientation so the troops form up
		// along a fixed direction and settle, instead of orbiting the objective (Phase 27).
		SquadSubsystem->SetSquadObjective(SquadId, Order, Location, FormationRadius, GetActorForwardVector());
	}
}

float AAshGeneralCharacter::GetAttackRange() const
{
	return HeroConfig ? HeroConfig->AttackRange : FallbackAttackRange;
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

void AAshGeneralCharacter::ApplyHitReaction(const FVector& SourceLocation, int32 SourceId)
{
	if (bIsDead)
	{
		return;
	}

	// Pushed back ever so slightly (Phase 32): a brief launch away from the attacker.
	if (HitReactKnockbackSpeed > 0.f)
	{
		FVector Dir = GetActorLocation() - SourceLocation;
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			Dir = -GetActorForwardVector();
		}
		LaunchCharacter(Dir * HitReactKnockbackSpeed, /*bXYOverride=*/true, /*bZOverride=*/false);
	}

	ApplyStun(HitReactStunDuration, SourceId);
}

void AAshGeneralCharacter::ApplyStun(float Duration, int32 SourceId)
{
	if (bIsDead || Duration <= 0.f || !AbilitySystemComponent || !StunEffectClass)
	{
		return;
	}

	// Game-wide new-source immunity (UAshCombatRulesSettings): same attacker re-stuns freely, a different
	// attacker is locked out until the window elapses (prevents infinite stun-locking).
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const float Immunity = GetDefault<UAshCombatRulesSettings>()->NewStunSourceImmunity;
	const bool bSameSource = (SourceId != INDEX_NONE) && (SourceId == LastStunSourceId);
	if (!bSameSource && Immunity > 0.f && (Now - LastStunTime) < Immunity)
	{
		return; // still immune to a new stun source
	}
	LastStunSourceId = SourceId;
	LastStunTime = Now;

	// Apply the stun as GE_State_Stunned (grants Ash.State.Stunned for Duration via SetByCaller, expires on
	// its own). The StateTree tasks read IsStunned() to stop movement + skip orders; cancel the attack now.
	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(StunEffectClass, 1.f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(AshGameplayTags::Data_StunDuration, Duration);
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
	AbilitySystemComponent->CancelAllAbilities();
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
}

bool AAshGeneralCharacter::IsStunned() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(AshGameplayTags::State_Stunned);
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
		const float FormationRadius = HeroConfig ? HeroConfig->CombatFormationRadius : 250.f;
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
	const float EnemyRadius = HeroConfig ? HeroConfig->EnemyEngageRadius : 1500.f;
	const float StrongholdRadius = HeroConfig ? HeroConfig->StrongholdDetourRadius : 2500.f;

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
	const UAshAILODSettings* LODSettings = GetDefault<UAshAILODSettings>();

	const int32 Level = LODSettings ? LODSettings->ComputeLODLevel(DistanceToPlayer) : 0;
	CurrentLODLevel = Level;

	const float Interval = LODSettings ? LODSettings->GetUpdateInterval(Level) : 0.f;
	CurrentThinkInterval = Interval;

	if (AAshGeneralController* GeneralController = Cast<AAshGeneralController>(GetController()))
	{
		GeneralController->SetThinkInterval(Interval);
	}

	// At non-visible LOD the non-player hero keeps only its coarse strategic presence. Cull the whole visual
	// representation, not just ACharacter::GetMesh(): hero BPs may split body/face/hair/equipment across
	// extra mesh or primitive components, and those must disappear with the general LOD too.
	const int32 VisibleMaxLOD = LODSettings ? FMath::Clamp(LODSettings->GeneralVisibleMaxLOD, 0, 3) : 0;
	const bool bRenderVisuals = Level <= VisibleMaxLOD;
	const bool bUseActorHidden = LODSettings && LODSettings->bHideGeneralActorWhenCulled;
	const auto ApplyWidgetVisibility = [bRenderVisuals](UWidgetComponent* WidgetComponent)
	{
		if (!WidgetComponent)
		{
			return;
		}

		WidgetComponent->SetVisibility(bRenderVisuals, true);
		WidgetComponent->SetHiddenInGame(!bRenderVisuals, true);
		WidgetComponent->SetComponentTickEnabled(bRenderVisuals);
		WidgetComponent->SetTickWhenOffscreen(false);

		if (UUserWidget* Widget = WidgetComponent->GetUserWidgetObject())
		{
			Widget->SetVisibility(bRenderVisuals ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	if (bUseActorHidden && bRenderVisuals)
	{
		SetActorHiddenInGame(false);
	}

	ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors=*/true, [bRenderVisuals](UPrimitiveComponent* Component)
	{
		if (!Component || Component->IsA<UCapsuleComponent>())
		{
			return;
		}

		Component->SetVisibility(bRenderVisuals, true);
		Component->SetComponentTickEnabled(bRenderVisuals);
	});

	ForEachComponent<UChildActorComponent>(/*bIncludeFromChildActors=*/false, [bRenderVisuals](UChildActorComponent* Component)
	{
		if (AActor* ChildActor = Component ? Component->GetChildActor() : nullptr)
		{
			ChildActor->SetActorHiddenInGame(!bRenderVisuals);
		}
	});

	ForEachComponent<UWidgetComponent>(/*bIncludeFromChildActors=*/true, [&ApplyWidgetVisibility](UWidgetComponent* Component)
	{
		const UUserWidget* LiveWidget = Component ? Component->GetUserWidgetObject() : nullptr;
		const UClass* WidgetClass = Component ? Component->GetWidgetClass() : nullptr;
		const bool bIsUnitHealthBar = (LiveWidget && LiveWidget->IsA<UAshUnitHealthBarWidget>())
			|| (WidgetClass && WidgetClass->IsChildOf(UAshUnitHealthBarWidget::StaticClass()));
		const bool bNamedHealthBar = Component && Component->GetName().Contains(TEXT("HealthBar"));

		if (bIsUnitHealthBar || bNamedHealthBar)
		{
			ApplyWidgetVisibility(Component);
		}
	});

	ForEachComponent<UMeshComponent>(/*bIncludeFromChildActors=*/true, [bRenderVisuals](UMeshComponent* Component)
	{
		if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Component))
		{
			SkeletalMesh->VisibilityBasedAnimTickOption = bRenderVisuals
				? EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones
				: EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		}
	});

	if (bUseActorHidden && !bRenderVisuals)
	{
		SetActorHiddenInGame(true);
	}

	// Hide the health bar widget at LOD 2+ (far/very-far): mirrors how soldier health bars only
	// exist when the proxy actor is promoted (i.e. the soldier is near the player).
	if (HealthBarWidget)
	{
		ApplyWidgetVisibility(HealthBarWidget);
	}

	return Level;
}

void AAshGeneralCharacter::UpdatePlayerPathDisplacement(float DeltaTime, const APawn* PlayerPawn)
{
	if (bIsDead || DeltaTime <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	if (!bReturningToPlayerDisplacementBase && Now - LastPlayerPushTime > PlayerPushBaseHoldSeconds)
	{
		PlayerDisplacementBasePosition = GetActorLocation();
	}

	if (!bReturningToPlayerDisplacementBase
		&& FVector::DistSquared2D(GetActorLocation(), PlayerDisplacementBasePosition) > FMath::Square(MaxPlayerForcedDisplacement))
	{
		bReturningToPlayerDisplacementBase = true;
	}

	const bool bPlayerCanPush = PlayerPawn && PlayerPushRadius > 0.f && PlayerPushSpeed > 0.f;
	if (bPlayerCanPush && !bReturningToPlayerDisplacementBase)
	{
		FVector PlayerVelocity = PlayerPawn->GetVelocity();
		PlayerVelocity.Z = 0.f;
		if (PlayerVelocity.SizeSquared2D() > FMath::Square(10.f))
		{
			FVector FromPlayer = GetActorLocation() - PlayerPawn->GetActorLocation();
			FromPlayer.Z = 0.f;
			const float DistSq = FromPlayer.SizeSquared();
			if (DistSq < FMath::Square(PlayerPushRadius))
			{
				const float Dist = FMath::Sqrt(FMath::Max(DistSq, 1.f));
				const FVector RadialDir = FromPlayer / Dist;
				const FVector PlayerDir = PlayerVelocity.GetSafeNormal2D();
				const FVector PushDir = (RadialDir + PlayerDir * 0.35f).GetSafeNormal2D();
				const float PushAlpha = 1.f - (Dist / PlayerPushRadius);

				FHitResult Hit;
				AddActorWorldOffset(PushDir * (PlayerPushSpeed * PushAlpha * DeltaTime), true, &Hit);
				LastPlayerPushTime = Now;

				if (FVector::DistSquared2D(GetActorLocation(), PlayerDisplacementBasePosition) > FMath::Square(MaxPlayerForcedDisplacement))
				{
					bReturningToPlayerDisplacementBase = true;
				}
			}
		}
	}

	if (bReturningToPlayerDisplacementBase)
	{
		FVector ToBase = PlayerDisplacementBasePosition - GetActorLocation();
		ToBase.Z = 0.f;
		const float Dist = ToBase.Size();
		if (Dist <= FMath::Max(25.f, ForcedReturnSpeed * DeltaTime))
		{
			FHitResult Hit;
			AddActorWorldOffset(ToBase, true, &Hit);
			PlayerDisplacementBasePosition = GetActorLocation();
			bReturningToPlayerDisplacementBase = false;
		}
		else if (Dist > KINDA_SMALL_NUMBER && ForcedReturnSpeed > 0.f)
		{
			FHitResult Hit;
			AddActorWorldOffset((ToBase / Dist) * ForcedReturnSpeed * DeltaTime, true, &Hit);
		}
	}
}

void AAshGeneralCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (bIsDead)
	{
		return;
	}

	// Keep the over-head bar in sync without a tick (ARCHITECTURE.md 18.3).
	if (HealthBarWidget && AttributeSet)
	{
		const float MaxHealth = FMath::Max(1.f, AttributeSet->GetMaxHealth());
		if (UAshUnitHealthBarWidget* Bar = Cast<UAshUnitHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
		{
			Bar->SetHealth(FMath::Clamp(Data.NewValue / MaxHealth, 0.f, 1.f));
		}
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
