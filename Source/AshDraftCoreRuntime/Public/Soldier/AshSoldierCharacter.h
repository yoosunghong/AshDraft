// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameFramework/Character.h"
#include "Teams/AshTeamTypes.h"
#include "Teams/AshTeamAgentInterface.h"
#include "AshSoldierCharacter.generated.h"

class UAshSoldierConfig;

/** Broadcast once when a soldier dies: the soldier and its team. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAshOnSoldierDied, AAshSoldierCharacter*, Soldier, EAshTeamId, Team);

/**
 * AAshSoldierCharacter
 *
 * The Phase 8 *temporary* Actor-based soldier (ARCHITECTURE.md 6.3 / PLAN Phase 8).
 * It exists to validate small-scale soldier combat — team identity, health, movement
 * toward an objective, simple attack, and death — before the data-oriented Mass Entity
 * implementation (Phases 9-10) replaces it.
 *
 * Design constraints honored even though it is throwaway:
 *  - No per-soldier Actor Tick (18.3): a repeating "think" timer at the config's
 *    ThinkInterval drives target acquisition, movement, and attacks.
 *  - No per-soldier Behavior Tree (7.4): the think loop is a few lines of local rules.
 *  - Team/health/combat numbers are data-driven via UAshSoldierConfig (17), and the
 *    fields map onto the future FAsh*Fragment layout so the Mass port is mechanical.
 *  - Targets are found through UAshSoldierSubsystem rather than a hard reference to
 *    other units (18.4).
 *
 * Health is a plain authoritative scalar (not GAS): soldiers become Mass entities with
 * a health fragment, not ability-system actors. Damage application is guarded by
 * authority so the model stays multiplayer-friendly (5.5 / 15).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshSoldierCharacter : public ACharacter, public IAshTeamAgentInterface
{
	GENERATED_BODY()

public:
	AAshSoldierCharacter();

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	//~IAshTeamAgentInterface
	virtual EAshTeamId GetAshTeamId() const override { return TeamId; }
	//~End of IAshTeamAgentInterface

	/** Team identity (BlueprintPure mirror of the interface). */
	UFUNCTION(BlueprintPure, Category = "Ash|Team")
	EAshTeamId GetTeamId() const { return TeamId; }

	/** Sets the soldier's team (e.g. when a base/spawner produces it). Safe pre/post BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Ash|Team")
	void SetTeamId(EAshTeamId NewTeam) { TeamId = NewTeam; }

	/** Current health. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetHealth() const { return CurrentHealth; }

	/** Maximum health (resolved from config, else fallback). */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	float GetMaxHealth() const;

	/** True once health has reached zero and death has been handled. */
	UFUNCTION(BlueprintPure, Category = "Ash|Health")
	bool IsDead() const { return bIsDead; }

	/**
	 * Applies damage from an instigator. Authority-guarded. Reduces health and triggers
	 * death at zero. The prototype's only damage source is another soldier's attack, but
	 * this is the single entry point future systems (general, capture) can call too.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ash|Health")
	void ReceiveDamage(float Amount, AActor* InInstigator);

	/** Orders this soldier to advance toward a world location when it has no closer target. */
	UFUNCTION(BlueprintCallable, Category = "Ash|Soldier")
	void SetObjectiveLocation(const FVector& InLocation);

	/** Fired when this soldier dies. Squad/base/commander systems subscribe (event-driven). */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Health")
	FAshOnSoldierDied OnSoldierDied;

protected:
	/** Periodic local-rules update (timer-driven, replaces Tick). */
	void Think();

	/** Validates the current target and, if needed, asks the subsystem for the nearest hostile soldier. */
	void AcquireTarget();

	/** Issues a move request toward GoalActor (if set) or GoalLocation, debounced against the active goal. */
	void MoveToward(AActor* GoalActor, const FVector& GoalLocation);

	/** Attempts an attack against the current target if in range and off cooldown. */
	void TryAttack();

	/** One-time death handling: stop movement, disable collision, notify base, broadcast, despawn. */
	void HandleDeath(AActor* Killer);

	/** Effective config value resolvers (Config if set, else inline fallback). */
	float ResolveMaxHealth() const;
	float ResolveMoveSpeed() const;
	float ResolveAttackRange() const;
	float ResolveAttackDamage() const;
	float ResolveAttackInterval() const;
	float ResolveDetectionRadius() const;
	float ResolveThinkInterval() const;
	float ResolveAcceptanceRadius() const;

	/** Draws health/state text above the soldier when bShowDebug. */
	void DrawDebug() const;

	/** Data-driven tuning. When null, inline Fallback* values are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier")
	TObjectPtr<UAshSoldierConfig> Config;

	/** Team identity. Set per-instance or by the spawner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|Team", meta = (AllowPrivateAccess = "true"))
	EAshTeamId TeamId = EAshTeamId::Ally;

	/** Inline fallback stats used when Config is unset (mirror UAshSoldierConfig defaults). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Fallback", meta = (ClampMin = "1.0"))
	float FallbackMaxHealth = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Fallback", meta = (ClampMin = "0.0"))
	float FallbackMoveSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Fallback", meta = (ClampMin = "0.0"))
	float FallbackAttackRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Fallback", meta = (ClampMin = "0.0"))
	float FallbackAttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Fallback", meta = (ClampMin = "0.05"))
	float FallbackAttackInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Fallback", meta = (ClampMin = "0.0"))
	float FallbackDetectionRadius = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Fallback", meta = (ClampMin = "0.05"))
	float FallbackThinkInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Soldier|Fallback", meta = (ClampMin = "0.0"))
	float FallbackAcceptanceRadius = 80.f;

	/** Draw health/state text above the soldier for verification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|Soldier|Debug", meta = (AllowPrivateAccess = "true"))
	bool bShowDebug = false;

private:
	/** Current health pool. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Ash|Health")
	float CurrentHealth = 0.f;

	/** Current engagement target (a hostile soldier). Weak: it may die independently. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AAshSoldierCharacter> CurrentTarget;

	/** Fallback advance goal when there is no target. Defaults to the spawn location. */
	FVector ObjectiveLocation = FVector::ZeroVector;
	bool bHasObjective = false;

	/** The actor the active move request is following, to debounce repeated MoveTo calls. */
	TWeakObjectPtr<AActor> ActiveMoveGoalActor;
	FVector ActiveMoveGoalLocation = FVector::ZeroVector;
	bool bHasActiveMove = false;

	/** World time (s) of the last successful attack; gates AttackInterval. Large negative = ready. */
	float LastAttackTime = -10000.f;

	/** Repeating think timer (replaces Tick). */
	FTimerHandle ThinkTimerHandle;

	/** Guards one-time death handling. */
	bool bIsDead = false;
};
