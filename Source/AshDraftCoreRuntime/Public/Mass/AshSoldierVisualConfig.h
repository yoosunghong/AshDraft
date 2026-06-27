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

	/** Montage played when this unit lands an attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Montage played when this unit is hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|UnitVisual|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;
};
