// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Mass/EntityHandle.h"
#include "AI/AshGeneralTypes.h"
#include "AI/AshSquadTypes.h"
#include "Teams/AshTeamTypes.h"
#include "Teams/AshTeamAgentInterface.h"
#include "AshGeneralCharacter.generated.h"

class UAshAbilitySystemComponent;
class UAshAttributeSet;
class UAshGameplayAbility;
class UAshGeneralConfig;
class AAshBaseActor;
struct FOnAttributeChangeData;

/** Broadcast once when a general's health first reaches zero (event-driven death). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAshOnGeneralCharacterDied, AAshGeneralCharacter*, General);

/**
 * AAshGeneralCharacter
 *
 * The operational AI layer of the hierarchy (Phase 22, ARCHITECTURE.md 6.2 / 8): a high-fidelity,
 * team-agnostic Character — not a Mass entity — that takes the commander's strategic order and
 * executes it on the ground while commanding a body of Mass soldiers. It unifies and replaces the
 * Phase 5 enemy general (same GAS foundation + event-driven death), but is driven by a **StateTree**
 * (via AAshGeneralController) instead of a Behavior Tree.
 *
 * Responsibilities:
 *  - **Troops:** on BeginPlay it spawns Config->TroopCount Mass soldiers into a squad it owns and
 *    registers that squad + itself with the squad / general subsystems.
 *  - **Following:** each operational tick its StateTree calls PublishSquadObjective so the troops'
 *    shared objective tracks the general's live position (the soldiers' existing follow/engage/return
 *    behavior then keeps them within FormationRadius — Phase 20/20.1).
 *  - **Sub-objectives:** it senses higher-priority threats (a hostile actor in range, a hostile base
 *    in its path) on a cadence; the StateTree conditions read HasThreat() to preempt the commander
 *    order and resume it afterward.
 *  - **AI-LOD:** UpdateThinkLOD throttles its StateTree think-rate by distance to the player.
 *
 * No per-frame Actor Tick (CLAUDE.md 18.3): everything is StateTree-, timer-, GAS- or delegate-driven.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshGeneralCharacter : public ACharacter, public IAbilitySystemInterface, public IAshTeamAgentInterface
{
	GENERATED_BODY()

public:
	AAshGeneralCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	//~APawn interface
	virtual void PossessedBy(AController* NewController) override;
	//~End of APawn interface

	//~IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~End of IAbilitySystemInterface

	//~IAshTeamAgentInterface
	virtual EAshTeamId GetAshTeamId() const override { return TeamId; }
	//~End of IAshTeamAgentInterface

	/** Team identity. */
	UFUNCTION(BlueprintPure, Category = "Ash|Team")
	EAshTeamId GetTeamId() const { return TeamId; }

	/** Data-driven archetype (troops, radii, StateTree, LOD). May be null (inline fallbacks used). */
	UFUNCTION(BlueprintPure, Category = "Ash|General")
	UAshGeneralConfig* GetConfig() const { return Config; }

	/** Squad id this general commands. */
	UFUNCTION(BlueprintPure, Category = "Ash|General")
	int32 GetSquadId() const { return SquadId; }

	/** Current/max health from the GAS attribute set. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetHealth() const;
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetMaxHealth() const;
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	bool IsDead() const { return bIsDead; }

	/** Fired when this general dies. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Health")
	FAshOnGeneralCharacterDied OnGeneralDied;

	// --- Operational AI surface (called by the controller / StateTree nodes) ---

	/** Runs the config StateTree and applies the initial think-rate. Called by the controller on possess. */
	void StartOperationalAI();

	/** Stores the commander's strategic order (read by the ExecuteOrder StateTree task). */
	void SetCommanderOrder(EAshSquadOrder Order, const FVector& ObjectiveLocation);

	UFUNCTION(BlueprintPure, Category = "Ash|General")
	EAshSquadOrder GetCommanderOrder() const { return CommanderOrder; }

	UFUNCTION(BlueprintPure, Category = "Ash|General")
	FVector GetCommanderObjective() const { return CommanderObjective; }

	UFUNCTION(BlueprintPure, Category = "Ash|General")
	bool HasCommanderObjective() const { return bHasCommanderObjective; }

	/**
	 * Publishes Location as the troops' shared objective (with the config FormationRadius), so the
	 * squad follows the general. The StateTree tasks call this every tick with the general's own
	 * location (or its move target), realizing "soldiers move within a range around the general".
	 */
	void PublishSquadObjective(const FVector& Location, EAshSquadOrder Order);

	/** Convenience: publish the general's own location as the follow objective. */
	void PublishFollowObjective(EAshSquadOrder Order) { PublishSquadObjective(GetActorLocation(), Order); }

	/** Reach (cm) the general uses to decide it can strike a sensed enemy. */
	UFUNCTION(BlueprintPure, Category = "Ash|General")
	float GetAttackRange() const;

	/** Triggers the general's basic attack via the shared input-tag activation path (GAS). */
	void TriggerBasicAttack();

	/** Applies plain Mass-soldier damage to this general's GAS health pool. */
	void ReceiveSoldierDamage(float Amount, AActor* DamageInstigator);

	// --- Threat sensing (drives the StateTree sub-objective preemptions) ---

	/** True if the general currently senses a threat of the given type (refreshes the cache if stale). */
	UFUNCTION(BlueprintPure, Category = "Ash|General")
	bool HasThreat(EAshThreatType ThreatType);

	/** Last-sensed hostile actor within EnemyEngageRadius (null if none). */
	UFUNCTION(BlueprintPure, Category = "Ash|General")
	AActor* GetSensedEnemy() const { return SensedEnemy.Get(); }

	/** Last-sensed hostile base within StrongholdDetourRadius (null if none). */
	UFUNCTION(BlueprintPure, Category = "Ash|General")
	AActor* GetSensedStronghold() const;

	/** Finds and locks a replacement hostile actor near this general, excluding ExcludedActor. */
	bool TryRetargetSensedEnemy(AActor* ExcludedActor, float SearchRadius);

	/** True when Actor is the local player's pawn. Used by StateTree chase-reset logic. */
	bool IsPlayerEnemyActor(const AActor* Actor) const;

	/** Publishes a tighter combat objective so soldiers enter the skirmish instead of idling behind. */
	void PublishCombatObjectiveNear(AActor* Enemy);

	// --- Actor AI-LOD ---

	/** Classifies LOD from player distance, applies the matching think-rate, returns the level. */
	int32 UpdateThinkLOD(float DistanceToPlayer);

protected:
	/** Binds actor info, seeds attributes, and grants abilities (authority only). */
	void InitializeAbilitySystem();
	void InitializeAttributes();
	void GrantDefaultAbilities();

	/** Spawns the general's troops and registers its squad (BeginPlay). */
	void SpawnTroops();

	/** Recomputes the sensed enemy / stronghold from the registries (cheap, throttled by think-rate). */
	void RefreshSensing();

	/** Health-changed handler: triggers death at zero. */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/** One-time death handling: stop the StateTree, disable collision/movement, broadcast, unregister. */
	void HandleDeath();

private:
	/** GAS ability system component owned by this general (PoC: ASC on the avatar). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAshAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAshAttributeSet> AttributeSet;

	/** Data-driven archetype. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAshGeneralConfig> Config;

	/** Team identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Team", meta = (AllowPrivateAccess = "true"))
	EAshTeamId TeamId = EAshTeamId::Enemy;

	/** Abilities granted on possession (each carries the input tag it binds to). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UAshGameplayAbility>> DefaultAbilities;

	/** Initial / maximum attributes (used when no other init is wired). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxHealth = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InitialAttackPower = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InitialDefense = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Attributes", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxStamina = 100.f;

	/** Seconds the body persists after death before being destroyed (0 = keep). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Health", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DeathLifeSpan = 5.f;

	/** Inline fallback attack range when Config is null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|General", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FallbackAttackRange = 175.f;

	// --- Runtime state ---

	/** Mass soldier entities this general owns (destroyed on EndPlay). */
	TArray<FMassEntityHandle> TroopEntities;

	/** Squad id stamped on the troops (allocated on spawn). */
	int32 SquadId = INDEX_NONE;

	/** Id in UAshGeneralSubsystem. */
	int32 GeneralId = INDEX_NONE;

	/** Commander's current strategic order + objective. */
	EAshSquadOrder CommanderOrder = EAshSquadOrder::None;
	FVector CommanderObjective = FVector::ZeroVector;
	bool bHasCommanderObjective = false;

	/** Sensing cache (refreshed when older than the current think interval). */
	TWeakObjectPtr<AActor> SensedEnemy;
	TWeakObjectPtr<AAshBaseActor> SensedStronghold;
	float LastSenseTime = -1.e30f;

	/** Current think interval (s) applied by AI-LOD; also gates the sensing-cache refresh. */
	float CurrentThinkInterval = 0.f;

	/** Current actor LOD level (0 = near .. 3 = far). */
	int32 CurrentLODLevel = 0;

	/** Guards. */
	bool bAbilitiesGranted = false;
	bool bIsDead = false;
};
