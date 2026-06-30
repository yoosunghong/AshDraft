// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "AshSoldierVisualConfig.generated.h"

class UAnimInstance;
class UAnimMontage;
class USkeletalMesh;

/**
 * UAshSoldierVisualConfig
 *
 * The data-driven *visual set* for one soldier/unit type (PLAN Phase 15, ARCHITECTURE.md 13/17).
 * It holds everything needed to dress a representation proxy at runtime: the skeletal mesh, the
 * Animation Blueprint class, the mesh's local transform correction, and the combat montages.
 *
 * This deliberately keeps visuals OUT of the proxy Blueprint. AAshSoldierProxyActor (B_Soldier_Proxy)
 * is a single generic template with an empty SkeletalMeshComponent; the representation processor
 * applies one of these assets to it per entity (UAshMassSoldierConfig::Visual). The result:
 *   - one source of truth for a unit's look (no mesh/anim authored on the Blueprint), and
 *   - new unit types (infantry / archer / cavalry) are added by authoring data, not Blueprints/code.
 *
 * Visual sets are intentionally separate from UAshMassSoldierConfig (the stat/"unit type" asset) so
 * a look can be shared across stat variants and vice versa (composition over a monolithic asset).
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshSoldierVisualConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Display name for this unit type, shown in the HUD's recently-struck-enemy panel (Phase 30).
	 * Empty falls back to a generic team-based label ("Enemy" etc.).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual")
	FText DisplayName;

	/** Skeletal mesh applied to the proxy body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Mesh")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Animation Blueprint (AnimInstance) class run on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Mesh")
	TSubclassOf<UAnimInstance> AnimClass;

	/** Local mesh offset so the feet sit on the entity position (UE skeletal meshes are usually
	 *  centred at the pelvis, ~90cm up). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Mesh")
	FVector MeshRelativeLocation = FVector(0.f, 0.f, -90.f);

	/** Local mesh rotation correction (UE skeletons commonly face +Y, needing -90 yaw to face +X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Mesh")
	FRotator MeshRelativeRotation = FRotator(0.f, -90.f, 0.f);

	/** Uniform mesh scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Mesh", meta = (ClampMin = "0.01"))
	float MeshScale = 1.f;

	/**
	 * Radius (cm) of the proxy's query-only hit capsule — the volume player/general melee sweeps test
	 * against so this unit can be struck (Phase 21). Sized to the body; not a physics-blocking capsule.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Hit", meta = (ClampMin = "0.0"))
	float CapsuleRadius = 34.f;

	/** Half-height (cm) of the proxy's hit capsule. The capsule sits feet-on-ground (centre at +half-height). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Hit", meta = (ClampMin = "0.0"))
	float CapsuleHalfHeight = 90.f;

	/**
	 * Single attack montage — the fallback / single-swing animation. Used when AttackComboMontages is empty
	 * (or a combo hit index is out of its range), so a unit that only needs one swing authors just this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/**
	 * Per-hit combo montages (Phase 29): index 0 plays for the 1st hit of an attack cycle, index 1 for the
	 * 2nd, index 2 for the 3rd. A soldier lands up to 3 hits in one cycle (chance scales with its general's
	 * morale), and the representation proxy plays AttackComboMontages[HitIndex] for each landed hit, falling
	 * back to AttackMontage when the array is empty or shorter than the hit index. Author e.g. Attack_C as
	 * hit 1 and Attack_C_SetB as hit 2 (the two Paragon minion attack variants) for visual variety.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Animation")
	TArray<TObjectPtr<UAnimMontage>> AttackComboMontages;

	/** Montage played when this unit is hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	/**
	 * Montage played once when this unit dies (Phase 27/29). Converted to a **montage** for consistency with
	 * the attack/hit animations (Phase 29): the representation proxy plays it via Montage_Play the frame the
	 * soldier's health hits zero, and sets UAshSoldierAnimInstance::bIsDead so the AnimBP can transition to a
	 * **Dead state** that holds the downed pose for the corpse window (montages blend back out on their own,
	 * so the held pose is the AnimBP Dead state's job). The proxy clears bIsDead + stops montages when the
	 * pooled body is recycled for a live soldier.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;
};
