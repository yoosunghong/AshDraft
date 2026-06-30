// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "AshHeroConfig.generated.h"

class AAshGeneralCharacter;
class UAshMassSoldierConfig;
class UStateTree;

/**
 * UAshHeroConfig
 *
 * One data asset per playable hero archetype (e.g. DA_Hero_Knight, DA_Hero_Ranger).
 * Allied and enemy generals are non-player instances of the same hero archetype. "General" is a
 * role name for ally/enemy heroes commanded by AI, not a separate data type.
 *
 * Usage:
 *  - Player-controlled hero:  base stats from this asset + FAshHeroStatBonuses bonuses (future save game)
 *  - AI-controlled ally/enemy hero: base stats from this asset only (no bonuses), spawned as AICharacterClass
 *
 * All stat values are fed into GAS via SetNumericAttributeBase so GE modifiers (buffs/debuffs) layer
 * correctly on top at runtime.
 */
UCLASS(BlueprintType)
class ASHDRAFTCORERUNTIME_API UAshHeroConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------------------
	// Identity

	/** Localised display name used in UI (hero select, HUD target panel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero")
	FText HeroName;

	/** Optional portrait texture for hero-select / status UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero")
	TSoftObjectPtr<UTexture2D> Portrait;

	// -----------------------------------------------------------------------------------
	// Base stats
	//
	// These are the "AI-tier" values: what the hero is at level 0 / without any player
	// upgrades.  Player progression (FAshHeroStatBonuses) is added on top in the hero
	// character's InitializeAttributes().

	/** Base maximum health (no bonuses). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Stats", meta = (ClampMin = "1.0"))
	float BaseMaxHealth = 100.f;

	/** Base attack power fed into the GAS damage execution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Stats", meta = (ClampMin = "0.0"))
	float BaseAttackPower = 25.f;

	/** Base defense subtracted from incoming raw damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Stats", meta = (ClampMin = "0.0"))
	float BaseDefense = 5.f;

	/** Base maximum stamina (the secondary pool; shown as "mana" in the HUD). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Stats", meta = (ClampMin = "1.0"))
	float BaseMaxStamina = 100.f;

	// -----------------------------------------------------------------------------------
	// Non-player hero command profile

	/** Unit type this non-player hero spawns for its squad. Null -> the spawn library's inline fallbacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Troops")
	TObjectPtr<UAshMassSoldierConfig> SoldierConfig;

	/** Number of 5-soldier fireteams this non-player hero commands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Troops", meta = (ClampMin = "0"))
	int32 TroopCount = 6;

	/** Radius (cm) of the area the squads are distributed across when first spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Troops", meta = (ClampMin = "0.0"))
	float TroopSpawnRadius = 1200.f;

	/** Radius (cm) troops form up within around the non-player hero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Troops", meta = (ClampMin = "0.0"))
	float FormationRadius = 700.f;

	/** The non-player hero's morale level at spawn (1..MaxMoraleLevel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Morale", meta = (ClampMin = "1"))
	int32 InitialMoraleLevel = 3;

	/** Top of the morale scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Morale", meta = (ClampMin = "1"))
	int32 MaxMoraleLevel = 5;

	/** Chance (0..1) a soldier extends an attack to a 2-hit combo at full morale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Morale", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxTwoHitComboChance = 0.20f;

	/** Chance (0..1) a soldier extends an attack to a full 3-hit combo at full morale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Morale", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxThreeHitComboChance = 0.10f;

	/** Distance (cm) the non-player hero is considered arrived at its commander objective. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Behavior", meta = (ClampMin = "0.0"))
	float ObjectiveAcceptanceRadius = 400.f;

	/** Distance (cm) within which a hostile actor preempts the commander order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Behavior", meta = (ClampMin = "0.0"))
	float EnemyEngageRadius = 1500.f;

	/** Distance (cm) within which a hostile base preempts the commander order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Behavior", meta = (ClampMin = "0.0"))
	float StrongholdDetourRadius = 2500.f;

	/** Reach (cm) at which the non-player hero triggers its melee attack on a sensed enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Behavior", meta = (ClampMin = "0.0"))
	float AttackRange = 175.f;

	/** Seconds a non-player hero may tunnel on the player before finding another nearby target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Behavior", meta = (ClampMin = "0.0"))
	float PlayerChaseRetargetSeconds = 5.f;

	/** Radius (cm) used when finding a replacement hostile after an extended player chase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Behavior", meta = (ClampMin = "0.0"))
	float PlayerChaseRetargetRadius = 2200.f;

	/** Formation radius (cm) used while soldiers are pulled into an active hero-vs-hero fight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Troops", meta = (ClampMin = "0.0"))
	float CombatFormationRadius = 250.f;

	/** Distance (cm) at which this hero's approach to a hostile non-player hero triggers a battle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Battle", meta = (ClampMin = "0.0"))
	float BattleEncounterRadius = 3000.f;

	/** Radius (cm) of the circle the squad duels are spread around. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Battle", meta = (ClampMin = "0.0"))
	float DuelRingRadius = 1400.f;

	/** Cap on how many friendly fireteams may pile onto one enemy fireteam. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Battle", meta = (ClampMin = "1"))
	int32 MaxAttackersPerEnemyFireteam = 2;

	/** Closest fireteams kept as a personal guard around the non-player hero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Battle", meta = (ClampMin = "0"))
	int32 GuardFireteamCount = 1;

	/** Smoothing (0..1) applied to the battle centre between replans. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Battle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RingRecenterSmoothing = 0.25f;

	/** Seconds between engagement replans. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|Battle", meta = (ClampMin = "0.05"))
	float ReplanPeriod = 0.5f;

	/** StateTree asset that drives non-player hero operational decisions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero|Command|AI")
	TObjectPtr<UStateTree> StateTree;

	// -----------------------------------------------------------------------------------
	// AI representation
	//
	// When this hero archetype appears as an enemy general (same hero, bot-controlled,
	// base stats only), place an instance of this class and assign this asset to its
	// HeroConfig slot.  Leaving this null is fine — it is informational for designers and
	// spawn systems; the class does not auto-spawn anything.

	/** Character class to place/spawn when this hero archetype is AI-controlled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ash|Hero")
	TSubclassOf<AAshGeneralCharacter> AICharacterClass;
};
