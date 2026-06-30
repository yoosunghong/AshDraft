// Copyright YoosungHong. All Rights Reserved.

#include "Mass/AshSoldierProxyActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Mass/AshSoldierAnimInstance.h"
#include "Mass/AshSoldierFragments.h"
#include "Mass/AshSoldierVisualConfig.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "UI/AshCombatFeedSubsystem.h"
#include "UI/AshSoldierHealthBarWidget.h"
#include "UI/AshUnitHealthBarWidget.h"

AAshSoldierProxyActor::AAshSoldierProxyActor()
{
	// A proxy is a passive view driven by the representation processor; never ticks itself
	// (ARCHITECTURE.md 18.3). The skeletal mesh still evaluates its own pose, but only while
	// rendered (set below) so off-screen proxies stay cheap.
	PrimaryActorTick.bCanEverTick = false;

	DefaultHealthBarWidgetClass = UAshSoldierHealthBarWidget::StaticClass();

	// Plain scene root carries the entity's *facing*; the mesh hangs under it with its own
	// art-correction transform (set by ConfigureVisual). Keeping these on separate components is
	// what stops SyncFromEntity's SetActorRotation from clobbering the mesh's yaw offset.
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootScene);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetMobility(EComponentMobility::Movable);
	MeshComponent->SetCanEverAffectNavigation(false);
	// Don't burn animation cost on proxies the player can't see (ARCHITECTURE.md 13/18).
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	// Query-only hit capsule: the target for player/general melee sweeps (Phase 21). It overlaps the
	// Pawn channel only, so attack sweeps (SweepMultiByChannel on ECC_Pawn) find it, but it never
	// blocks movement, never simulates physics, and never pushes other soldiers (spacing stays the
	// data-oriented steering separation — adding physics push here would re-introduce crowd shaking).
	HitCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitCapsule"));
	HitCapsule->SetupAttachment(RootScene);
	HitCapsule->SetCapsuleSize(34.f, 90.f);
	HitCapsule->SetRelativeLocation(FVector(0.f, 0.f, 90.f)); // feet on the entity position
	HitCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision); // enabled on assignment
	HitCapsule->SetCollisionObjectType(ECC_Pawn);
	HitCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitCapsule->SetGenerateOverlapEvents(false); // queried by sweeps, not an event source
	HitCapsule->SetCanEverAffectNavigation(false);

	// Over-head unit health bar (Phase 30). Screen-space so it always faces the camera and stays a
	// readable size regardless of distance. The widget CLASS is assigned on B_Soldier_Proxy (a WBP child
	// of UAshUnitHealthBarWidget); left empty here so the proxy stays a data-driven template. Hidden with
	// the actor when pooled, so only promoted (rendered) soldiers show a bar.
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootScene);
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 210.f)); // above the head
	HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarWidget->SetGenerateOverlapEvents(false);
	HealthBarWidget->SetCanEverAffectNavigation(false);
	HealthBarWidget->SetTickWhenOffscreen(false);
	ApplyHealthBarPresentation();
	EnsureHealthBarWidgetClass();

	// Idle until the pool assigns this proxy to an entity.
	SetActorHiddenInGame(true);
}

void AAshSoldierProxyActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyHealthBarPresentation();
	EnsureHealthBarWidgetClass();
}

void AAshSoldierProxyActor::ApplyHealthBarPresentation()
{
	if (HealthBarWidget)
	{
		const FVector2D ClampedDrawSize(
			FMath::Clamp(HealthBarDrawSize.X, 1.f, MaxHealthBarDrawSize.X),
			FMath::Clamp(HealthBarDrawSize.Y, 1.f, MaxHealthBarDrawSize.Y));
		HealthBarWidget->SetDrawAtDesiredSize(false);
		HealthBarWidget->SetDrawSize(ClampedDrawSize);
	}
}

void AAshSoldierProxyActor::EnsureHealthBarWidgetClass()
{
	if (!HealthBarWidget)
	{
		return;
	}

	TSubclassOf<UUserWidget> SoldierWidgetClass = DefaultHealthBarWidgetClass;
	if (!SoldierWidgetClass || !SoldierWidgetClass->IsChildOf(UAshSoldierHealthBarWidget::StaticClass()))
	{
		SoldierWidgetClass = UAshSoldierHealthBarWidget::StaticClass();
	}

	if (HealthBarWidget->GetWidgetClass() == SoldierWidgetClass)
	{
		return;
	}

	HealthBarWidget->SetWidgetClass(SoldierWidgetClass);
	HealthBarWidget->InitWidget();
}

bool AAshSoldierProxyActor::ShouldShowHealthBar() const
{
	return RepresentedTeam == EAshTeamId::Enemy;
}

void AAshSoldierProxyActor::ApplyHealthBarVisibility()
{
	if (!HealthBarWidget)
	{
		return;
	}

	const bool bShowHealthBar = ShouldShowHealthBar();
	HealthBarWidget->SetVisibility(bShowHealthBar, true);
	HealthBarWidget->SetHiddenInGame(!bShowHealthBar, true);

	if (UUserWidget* Widget = HealthBarWidget->GetUserWidgetObject())
	{
		Widget->SetVisibility(bShowHealthBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void AAshSoldierProxyActor::AssignToEntity(FMassEntityHandle Entity, EAshTeamId Team)
{
	RepresentedEntity = Entity;
	RepresentedTeam = Team;
	SetActorHiddenInGame(false);
	EnsureHealthBarWidgetClass();
	ApplyHealthBarVisibility();

	// Become a hittable target only while representing a live soldier (pooled proxies must not be
	// stray hits under the player's attack sweep). Re-assert overlap-only collision at runtime (after any
	// Blueprint defaults on B_Soldier_Proxy have applied) so the capsule can NEVER block movement: a
	// blocking preset here would wall the player in when surrounded ("player can't move/rotate"). Crowd
	// spacing must stay the data-oriented steering separation, never physical blocking.
	if (HitCapsule)
	{
		HitCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		HitCapsule->SetCollisionObjectType(ECC_Pawn);
		HitCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		HitCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}

	// Pooled proxies are recycled across soldiers: a proxy that was a corpse still has bIsDead set on its
	// AnimBP (Dead state holding the downed pose). Clear it so the recycled body leaves the Dead state and
	// animates as a live soldier again (Phase 29). The death montage is also stopped in ClearAssignment.
	if (UAshSoldierAnimInstance* SoldierAnim = MeshComponent ? Cast<UAshSoldierAnimInstance>(MeshComponent->GetAnimInstance()) : nullptr)
	{
		SoldierAnim->bIsDead = false;
	}

	if (UAshUnitHealthBarWidget* Bar = HealthBarWidget ? Cast<UAshUnitHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()) : nullptr)
	{
		Bar->SetUnitName(FText::GetEmpty());
	}
}

void AAshSoldierProxyActor::ClearAssignment()
{
	// Stop any montage still playing (incl. a death montage) and clear the dead flag so a recycled proxy
	// starts clean on its next entity (Phase 29).
	if (MeshComponent)
	{
		if (UAnimInstance* Anim = MeshComponent->GetAnimInstance())
		{
			Anim->Montage_Stop(0.f);
		}
		if (UAshSoldierAnimInstance* SoldierAnim = Cast<UAshSoldierAnimInstance>(MeshComponent->GetAnimInstance()))
		{
			SoldierAnim->bIsDead = false;
		}
	}

	RepresentedEntity.Reset();
	RepresentedTeam = EAshTeamId::Neutral;
	SetActorHiddenInGame(true);

	// Parked proxies are not hittable.
	if (HitCapsule)
	{
		HitCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	ApplyHealthBarVisibility();

	if (UAshUnitHealthBarWidget* Bar = HealthBarWidget ? Cast<UAshUnitHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()) : nullptr)
	{
		Bar->SetUnitName(FText::GetEmpty());
	}
}

void AAshSoldierProxyActor::SyncFromEntity(const FVector& Position, float FacingYaw, const FVector& Velocity, float HealthFraction,
	bool bInCombatStance)
{
	// Mass is the authority; the proxy follows the entity's position and faces the Mass-authoritative
	// heading.
	SetActorLocation(Position);

	// Drive the over-head health bar (Phase 30). The screen-space user widget is created lazily on first
	// render, so guard the cast; team sets the colour (cyan friendly / red enemy), fraction the fill.
	ApplyHealthBarVisibility();
	if (HealthBarWidget)
	{
		if (UAshUnitHealthBarWidget* Bar = Cast<UAshUnitHealthBarWidget>(HealthBarWidget->GetUserWidgetObject()))
		{
			if (ShouldShowHealthBar())
			{
				Bar->SetTeam(RepresentedTeam);
				Bar->SetHealth(HealthFraction);
			}
		}
	}

	// Facing comes from Mass (target-aware + interpolated), NOT from raw velocity — so a soldier that
	// has stopped in melee still faces its enemy instead of keeping a stale travel heading (Phase 20).
	// Rotates the scene root only; the mesh keeps its art-correction yaw (see RootScene).
	SetActorRotation(FRotator(0.f, FacingYaw, 0.f));

	const FVector Planar(Velocity.X, Velocity.Y, 0.f);
	const float PlanarSpeed = Planar.Size();
	const bool bMoving = PlanarSpeed > FacingSpeedThreshold;

	// Travel direction relative to facing (degrees, -180..180): 0 = walking forward, ±180 = backpedalling
	// (kiting retreat while still facing the enemy), ±90 = strafing. Lets a 2D blendspace pick a
	// backward-walk clip instead of a forward one. Computed from the planar velocity and the facing yaw
	// by hand (atan2 of the cross/dot) so the proxy needs no extra anim-library dependency.
	float Direction = 0.f;
	if (bMoving)
	{
		const float YawRad = FMath::DegreesToRadians(FacingYaw);
		const FVector2D Facing2D(FMath::Cos(YawRad), FMath::Sin(YawRad));
		const FVector2D Vel2D = FVector2D(Planar.X, Planar.Y).GetSafeNormal();
		const float Dot = FVector2D::DotProduct(Facing2D, Vel2D);
		const float Cross = Facing2D.X * Vel2D.Y - Facing2D.Y * Vel2D.X;
		Direction = FMath::RadiansToDegrees(FMath::Atan2(Cross, Dot));
	}

	// Feed Mass-authoritative locomotion to the AnimBP. The proxy has no movement component, so the
	// AnimBP can't read GetVelocity(); this is how Idle<->Move blends get a real speed. Only rendered
	// proxies have a live anim instance (OnlyTickPoseWhenRendered), so a null here is expected/cheap.
	if (UAshSoldierAnimInstance* SoldierAnim = Cast<UAshSoldierAnimInstance>(MeshComponent->GetAnimInstance()))
	{
		SoldierAnim->GroundSpeed = PlanarSpeed;
		SoldierAnim->Direction = Direction;
		SoldierAnim->bIsMoving = bMoving;
		SoldierAnim->bInCombatStance = bInCombatStance;
	}
}

FVector AAshSoldierProxyActor::GetSyncedLocation() const
{
	return GetActorLocation();
}

void AAshSoldierProxyActor::ConfigureVisual(const UAshSoldierVisualConfig* Visual)
{
	// Pooled proxies are reused across entities (and unit types); only re-dress when the visual
	// set actually changes. This is called every frame for promoted entities, so the early-out
	// keeps the common case free.
	if (Visual == CurrentVisual || !MeshComponent)
	{
		return;
	}
	CurrentVisual = Visual;

	if (!Visual)
	{
		return; // No data: keep whatever (if anything) the Blueprint authored.
	}

	MeshComponent->SetSkeletalMeshAsset(Visual->SkeletalMesh);
	MeshComponent->SetRelativeLocation(Visual->MeshRelativeLocation);
	MeshComponent->SetRelativeRotation(Visual->MeshRelativeRotation);
	MeshComponent->SetRelativeScale3D(FVector(Visual->MeshScale));

	// Anim class is applied after the mesh so the new AnimInstance binds to the right skeleton.
	if (Visual->AnimClass)
	{
		MeshComponent->SetAnimInstanceClass(Visual->AnimClass);
	}

	// Fit the hit capsule to this unit type's body so the melee sweep tests a sensible volume, and
	// keep its feet on the entity position (capsule centre at +half-height).
	if (HitCapsule)
	{
		HitCapsule->SetCapsuleSize(Visual->CapsuleRadius, Visual->CapsuleHalfHeight);
		HitCapsule->SetRelativeLocation(FVector(0.f, 0.f, Visual->CapsuleHalfHeight));
	}
}

void AAshSoldierProxyActor::ReceiveMeleeHit(float Damage, const AActor* _Instigator)
{
	// Mass is authoritative: route the actor-space hit back into the entity's health fragment. The
	// proxy is only the visible body / hit target; the combat + death processors do the rest.
	if (!RepresentedEntity.IsSet() || Damage <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(RepresentedEntity))
	{
		return;
	}

	if (FAshHealthFragment* Health = EntityManager.GetFragmentDataPtr<FAshHealthFragment>(RepresentedEntity))
	{
		Health->CurrentHealth = FMath::Max(0.f, Health->CurrentHealth - Damage);

		// Raise the one-shot hit-react event so the representation processor plays the flinch montage
		// this frame (same path the Mass combat processor uses).
		if (FAshCombatEventFragment* Event = EntityManager.GetFragmentDataPtr<FAshCombatEventFragment>(RepresentedEntity))
		{
			Event->bWasHitThisTick = true;
		}

		// HUD combat feed (Phase 30): record the player's strike on this Mass soldier. The feed ignores
		// non-player instigators, so AI-vs-soldier hits (e.g. a general's sweep) are harmlessly skipped.
		if (UAshCombatFeedSubsystem* Feed = UAshCombatFeedSubsystem::Get(this))
		{
			const float MaxHealth = FMath::Max(1.f, Health->MaxHealth);
			const FText UnitName = CurrentVisual ? CurrentVisual->DisplayName : FText::GetEmpty();
			Feed->ReportPlayerStrike(_Instigator, UnitName, RepresentedTeam,
				Health->CurrentHealth / MaxHealth, Health->CurrentHealth <= 0.f);
		}
	}
}

void AAshSoldierProxyActor::PlayAttackMontage(int32 ComboIndex)
{
	// Per-hit combo montage (Phase 29): use AttackComboMontages[ComboIndex] when authored, else fall back
	// to the single AttackMontage so single-swing units still animate.
	UAnimMontage* Montage = nullptr;
	if (CurrentVisual)
	{
		if (CurrentVisual->AttackComboMontages.IsValidIndex(ComboIndex) && CurrentVisual->AttackComboMontages[ComboIndex])
		{
			Montage = CurrentVisual->AttackComboMontages[ComboIndex];
		}
		else
		{
			Montage = CurrentVisual->AttackMontage;
		}
	}
	PlayMontage(Montage);
}

void AAshSoldierProxyActor::PlayHitReactMontage()
{
	PlayMontage(CurrentVisual ? CurrentVisual->HitReactMontage : nullptr);
}

void AAshSoldierProxyActor::PlayDeath()
{
	// A corpse must not be struck again or block anything.
	if (HitCapsule)
	{
		HitCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Death is now a montage for consistency with attack/hit (Phase 29). Montage_Play drives the death
	// motion; setting bIsDead lets the AnimBP transition to a Dead state that HOLDS the downed pose for the
	// corpse window (a montage blends back out on its own, so the held pose is the AnimBP state's job).
	if (UAshSoldierAnimInstance* SoldierAnim = MeshComponent ? Cast<UAshSoldierAnimInstance>(MeshComponent->GetAnimInstance()) : nullptr)
	{
		SoldierAnim->bIsDead = true;
	}
	PlayMontage(CurrentVisual ? CurrentVisual->DeathMontage : nullptr);
}

void AAshSoldierProxyActor::PlayMontage(UAnimMontage* Montage)
{
	if (!Montage || !MeshComponent)
	{
		return;
	}
	if (UAnimInstance* Anim = MeshComponent->GetAnimInstance())
	{
		Anim->Montage_Play(Montage);
	}
}
