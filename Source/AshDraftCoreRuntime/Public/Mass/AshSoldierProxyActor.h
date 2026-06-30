// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Mass/EntityHandle.h"
#include "Teams/AshTeamTypes.h"
#include "AshSoldierProxyActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UCapsuleComponent;
class UWidgetComponent;
class UUserWidget;
class UAnimMontage;
class UAshSoldierVisualConfig;

/**
 * AAshSoldierProxyActor
 *
 * Phase 13 hybrid representation (ARCHITECTURE.md 13 / 6.3): a lightweight Actor that gives
 * a *nearby* Mass soldier higher visual fidelity than a debug point. Most soldiers stay as
 * pure Mass entities; only LOD-0 soldiers near the player are "promoted" to one of these
 * proxies, which are pooled and recycled by UAshSoldierProxyPool / the representation
 * processor so the active Actor count stays bounded.
 *
 * The proxy is a dumb view: it has no AI, no Tick, and no combat. It mirrors the entity's
 * position/heading (and team colour / health) each frame from Mass, and can report its
 * transform back when demoted (ARCHITECTURE.md 13.3 state transfer). Mass remains the
 * authority.
 *
 * Phase 15 upgrades the body to a SkeletalMesh so soldiers animate, and makes the proxy a single
 * generic *template*: the skeletal mesh, AnimBP, mesh transform and montages are NOT authored on
 * the Blueprint — they are applied at runtime from a UAshSoldierVisualConfig (the entity's unit
 * type) via ConfigureVisual(). One proxy class therefore renders every unit type. The
 * representation processor plays the visual set's montages in response to the one-shot combat
 * events (FAshCombatEventFragment) the Mass combat processor raises — the proxy stays brainless.
 */
UCLASS()
class ASHDRAFTCORERUNTIME_API AAshSoldierProxyActor : public AActor
{
	GENERATED_BODY()

public:
	AAshSoldierProxyActor();

	/** Begins representing Entity; makes the proxy visible and records the handle. */
	void AssignToEntity(FMassEntityHandle Entity, EAshTeamId Team);

	/** Stops representing any entity; hides + parks the proxy for reuse by the pool. */
	void ClearAssignment();

	/** The entity this proxy currently represents (unset when pooled/idle). */
	FMassEntityHandle GetRepresentedEntity() const { return RepresentedEntity; }

	/** Team currently represented (for hit / friendly-fire classification). */
	EAshTeamId GetRepresentedTeam() const { return RepresentedTeam; }

	/**
	 * Applies an external melee hit (e.g. the hero/general's attack sweep) to the represented Mass
	 * entity (Phase 21). The proxy is the *hit target* for actor-space attacks, but Mass stays
	 * authoritative: this subtracts Damage from the entity's FAshHealthFragment via the entity
	 * manager and raises its hit-react event, so the death processor removes it on zero like any
	 * other casualty. No-op if pooled / the entity is gone. Caller is responsible for team checks.
	 */
	void ReceiveMeleeHit(float Damage, const AActor* Instigator);

	/** True while assigned to a live entity. */
	bool IsAssigned() const { return RepresentedEntity.IsSet(); }

	/**
	 * Pushes Mass state onto the proxy (ARCHITECTURE.md 13.3: Position, heading, Health). FacingYaw is
	 * the Mass-authoritative body yaw computed by the movement processor (target-aware + interpolated),
	 * so a stopped, attacking soldier still faces its enemy. Velocity is used only to drive locomotion
	 * animation speed, no longer to derive facing (Phase 20).
	 */
	void SyncFromEntity(const FVector& Position, float FacingYaw, const FVector& Velocity, float HealthFraction,
		bool bInCombatStance = false);

	/** Reads the proxy's current world location back out (for demotion transfer). */
	FVector GetSyncedLocation() const;

	/**
	 * Applies a unit type's visual set (skeletal mesh, AnimBP, mesh transform, montages) to this
	 * proxy. Cheap-and-idempotent: early-outs when Visual is unchanged, so the representation
	 * processor may call it every frame. Passing null leaves the current look untouched.
	 */
	void ConfigureVisual(const UAshSoldierVisualConfig* Visual);

	/**
	 * Plays the attack montage for combo hit ComboIndex (Phase 29): the current visual set's
	 * AttackComboMontages[ComboIndex] if authored, else the single AttackMontage. Driven by a Mass attack
	 * event (FAshCombatEventFragment::AttackComboIndex); no-op if no montage resolves.
	 */
	void PlayAttackMontage(int32 ComboIndex);

	/** Plays the current visual set's hit-react montage (no-op if none); driven by a Mass hit event. */
	void PlayHitReactMontage();

	/**
	 * Plays the current visual set's DeathMontage and flags the AnimBP as dead (Phase 27/29): Montage_Play
	 * drives the death motion and UAshSoldierAnimInstance::bIsDead lets the AnimBP transition to a Dead state
	 * that **holds the downed pose** for the corpse window (a montage alone blends back out). Also disables
	 * the query-only hit capsule so a corpse can't be struck again. Driven once by the representation
	 * processor the frame the entity starts dying. No-op if the visual set has no DeathMontage.
	 */
	void PlayDeath();

protected:
	//~AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;
	//~End of AActor interface

	/**
	 * Facing root. SyncFromEntity rotates THIS (not the mesh) to face movement, keeping the entity's
	 * heading separate from the mesh's art-correction offset. Making the mesh the root caused the
	 * two to fight: SetActorRotation clobbered the -90 yaw set on the mesh, so soldiers faced the
	 * wrong way relative to travel ("moonwalk"). The mesh hangs under this with its own relative
	 * transform from the visual set.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Proxy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> RootScene;

	/** Skeletal body of the proxied soldier. Left empty on the Blueprint; dressed by ConfigureVisual. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Proxy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	/**
	 * Query-only hit capsule (Phase 21). This is what player/general melee sweeps (ECC_Pawn) test
	 * against so a Mass soldier can be struck, WITHOUT giving it physics-blocking collision or a
	 * per-soldier physics body (which is the wrong tool at army scale and was the source of the crowd
	 * shaking). It overlaps the Pawn channel only — it never blocks movement and never pushes other
	 * soldiers (spacing stays the steering separation). Size comes from the visual config; collision
	 * is enabled only while assigned to a live entity. Visible on the Blueprint so it is tunable.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Proxy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> HitCapsule;

	/**
	 * Over-head unit health bar (Phase 30). A screen-space UWidgetComponent that shows the soldier's
	 * health (cyan for the player's side, red for the enemy). Because a proxy only exists while its
	 * entity is promoted near the player (representation LOD), the bar is automatically shown only
	 * "together with the mesh when it is rendered" and far soldiers pay no widget cost. The widget
	 * CLASS is assigned on the B_Soldier_Proxy Blueprint (a WBP child of UAshUnitHealthBarWidget); this
	 * component is empty by default so the proxy stays data-driven. SyncFromEntity pushes health/team in.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ash|Proxy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	/** Compact draw size for ordinary soldier health bars. Generals use their own larger component size. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Proxy|UI", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	FVector2D HealthBarDrawSize = FVector2D(30.f, 6.f);

	/** Upper bound applied to soldier bars so stale Blueprint defaults cannot make them hero-sized. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Proxy|UI", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	FVector2D MaxHealthBarDrawSize = FVector2D(30.f, 8.f);

	/**
	 * Soldier-specific widget class. Keep this on the native compact widget unless intentionally testing
	 * another soldier-only presentation; generals use their own larger widget component/class.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Proxy|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> DefaultHealthBarWidgetClass;

	/** Min planar speed (cm/s) before the proxy reorients to face its movement direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ash|Proxy", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FacingSpeedThreshold = 10.f;

private:
	/** Applies health-bar presentation defaults to the widget component. */
	void ApplyHealthBarPresentation();

	/** Ensures the health bar has a widget class even if the proxy Blueprint component was left unset. */
	void EnsureHealthBarWidgetClass();

	/** Shows ordinary soldier health only for enemy-team proxies. */
	bool ShouldShowHealthBar() const;

	/** Applies the current enemy-only visibility rule to the health bar component and widget. */
	void ApplyHealthBarVisibility();

	/** Plays a montage on the mesh's anim instance (no-op if null). */
	void PlayMontage(UAnimMontage* Montage);

private:
	/** Handle of the represented entity; unset means this proxy is idle in the pool. */
	FMassEntityHandle RepresentedEntity;

	/** Team currently represented (for colour/state). */
	EAshTeamId RepresentedTeam = EAshTeamId::Neutral;

	/** Visual set currently applied; cached so ConfigureVisual only re-dresses on a real change. */
	UPROPERTY(Transient)
	TObjectPtr<const UAshSoldierVisualConfig> CurrentVisual = nullptr;
};
