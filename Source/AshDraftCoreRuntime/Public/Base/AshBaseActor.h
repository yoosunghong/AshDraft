// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Teams/AshTeamTypes.h"
#include "AshBaseActor.generated.h"

class UAshBaseConfig;
class USphereComponent;

/** Ownership changed: Base, previous team, new team. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAshOnBaseOwnershipChanged, AAshBaseActor*, Base, EAshTeamId, OldTeam, EAshTeamId, NewTeam);

/** Durability changed: Base, current durability, max durability. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAshOnBaseDurabilityChanged, AAshBaseActor*, Base, float, Durability, float, MaxDurability);

/** Contested state changed: Base, now contested?. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAshOnBaseContestedChanged, AAshBaseActor*, Base, bool, bContested);

/**
 * AAshBaseActor
 *
 * A capturable battlefield stronghold (ARCHITECTURE.md 11). It owns a team, a
 * durability pool, and a spherical capture volume. The PoC uses a durability-based
 * capture model (11.2): while a hostile team holds the volume uncontested, durability
 * drains; at zero, ownership flips to that team and durability resets. With both
 * teams present the base is contested and frozen; with no attackers it recovers after
 * a delay.
 *
 * Because soldiers do not exist yet (Phase 8), capture is driven by *presence* of
 * team-tagged pawns in the volume (resolved via UAshTeamStatics). The forward-looking
 * hooks NotifyDefenderDied / ApplyCaptureDamage let Phase 8 soldiers drive durability
 * directly without changing this class.
 *
 * State changes are event-driven (delegates) so UI reads through them and the
 * commander AI reacts via UAshBaseSubsystem — no Actor Tick: the capture loop runs on
 * a repeating timer (ARCHITECTURE.md 16 / 18.3 / 18.4).
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshBaseActor : public AActor
{
	GENERATED_BODY()

public:
	AAshBaseActor();

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of AActor interface

	/** Current owning team. */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	EAshTeamId GetOwningTeam() const { return OwningTeam; }

	/** Current durability (absolute). */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	float GetDurability() const { return CurrentDurability; }

	/** Durability as a 0-1 fraction (safe against zero max). */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	float GetDurabilityNormalized() const;

	/** True when both a defending and an attacking team occupy the volume. */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	bool IsContested() const { return bContested; }

	/** Number of owning-team pawns currently in the capture volume. */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	int32 GetDefenderCount() const { return DefenderCount; }

	/** Number of pawns hostile to the owner currently in the capture volume. */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	int32 GetAttackerCount() const { return AttackerCount; }

	/**
	 * True if this base is a "main"/home base. The match loop (UAshMatchSubsystem) treats a
	 * player-side main base being captured by the enemy as an instant defeat (PLAN Phase 16).
	 * Designers flag the ally and enemy home bases on the placed instance.
	 */
	UFUNCTION(BlueprintPure, Category = "Ash|Base")
	bool IsMainBase() const { return bIsMainBase; }

	/**
	 * Forward-looking hook for Phase 8: a defender of DefenderTeam died at/around the
	 * base, draining durability. No-op if DefenderTeam is not the current owner.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ash|Base")
	void NotifyDefenderDied(EAshTeamId DefenderTeam);

	/** Directly reduce durability on behalf of an attacking team (soldier/general capture damage). */
	UFUNCTION(BlueprintCallable, Category = "Ash|Base")
	void ApplyCaptureDamage(EAshTeamId FromTeam, float Amount);

	/** Fired when this base changes owner. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Base")
	FAshOnBaseOwnershipChanged OnOwnershipChanged;

	/** Fired whenever durability changes. UI binds here (ARCHITECTURE.md 16). */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Base")
	FAshOnBaseDurabilityChanged OnDurabilityChanged;

	/** Fired when the contested flag flips. */
	UPROPERTY(BlueprintAssignable, Category = "Ash|Base")
	FAshOnBaseContestedChanged OnContestedChanged;

protected:
	/** UI placeholder hook (ARCHITECTURE.md 16): override in a Blueprint widget/base to react. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ash|Base", meta = (DisplayName = "On Durability Changed"))
	void K2_OnDurabilityChanged(float Durability, float MaxDurability);

	/** UI placeholder hook: override in a Blueprint to react to ownership flips. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ash|Base", meta = (DisplayName = "On Ownership Changed"))
	void K2_OnOwnershipChanged(EAshTeamId OldTeam, EAshTeamId NewTeam);

	/** Periodic capture-state evaluation (timer-driven). */
	void UpdateCaptureState();

	/** Recounts defenders/attackers from the current volume overlaps. */
	void RecountOccupants();

	/** Applies a durability delta, clamps, fires the changed events, and handles a flip at zero. */
	void ChangeDurability(float Delta);

	/** Performs the ownership flip to NewTeam and resets durability. */
	void SetOwningTeam(EAshTeamId NewTeam);

	/** Effective config value resolvers (Config if set, else inline fallback). */
	float ResolveMaxDurability() const;
	float ResolveCaptureRadius() const;

	/** Draws the capture volume + state when bShowDebug. Called from the update timer. */
	void DrawDebug() const;

	/** Capture volume root; overlaps with Pawns drive defender/attacker counts. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CaptureVolume;

	/** Data-driven tuning. When null, inline Fallback* values are used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base")
	TObjectPtr<UAshBaseConfig> Config;

	/** Team that owns the base at match start / current owner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (AllowPrivateAccess = "true"))
	EAshTeamId OwningTeam = EAshTeamId::Neutral;

	/**
	 * Marks this base as a main/home base for the match-flow loss condition (PLAN Phase 16).
	 * Set true on the ally and enemy home bases in the level; the neutral mid base stays false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base", meta = (AllowPrivateAccess = "true"))
	bool bIsMainBase = false;

	/** Inline fallback max durability when Config is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base|Fallback", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float FallbackMaxDurability = 100.f;

	/** Inline fallback capture radius (cm) when Config is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Base|Fallback", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float FallbackCaptureRadius = 600.f;

	/** Draw the capture volume + state on screen for debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ash|Base|Debug", meta = (AllowPrivateAccess = "true"))
	bool bShowDebug = false;

private:
	/** Current durability pool. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Ash|Base")
	float CurrentDurability = 0.f;

	/** Cached defender/attacker counts from the last recount. */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Ash|Base")
	int32 DefenderCount = 0;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Ash|Base")
	int32 AttackerCount = 0;

	/** The hostile team currently draining the base (the prospective new owner). */
	EAshTeamId CapturingTeam = EAshTeamId::Neutral;

	/** Contested = both a defending and attacking presence. */
	bool bContested = false;

	/** Seconds since the base last had any attacker presence (gates recovery). */
	float TimeSinceContested = 0.f;

	/** Repeating capture-evaluation timer (replaces Tick). */
	FTimerHandle CaptureTimerHandle;
};
