// Copyright YoosungHong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Teams/AshTeamTypes.h"

class UAshMassSoldierConfig;
class UWorld;

/**
 * AshMassSoldierSpawnLibrary
 *
 * One shared code path for batch-creating Mass soldier entities (ARCHITECTURE.md 7.2). The Phase 9
 * spawner (AAshMassSoldierSpawner) and the Phase 22 General (AAshGeneralCharacter) both create a
 * body of soldiers the same way — build the soldier archetype, batch-create entities, and seed each
 * one's FAsh* fragments from a UAshMassSoldierConfig (or inline fallbacks). Centralizing it keeps a
 * single source of truth for the soldier archetype + fragment seeding (CLAUDE.md DRY / 7.4).
 *
 * Plain C++ (not a UBlueprintFunctionLibrary) because the API traffics in FMassEntityHandle, which
 * is not a Blueprint type.
 */
namespace AshMassSoldierSpawn
{
	/** Inputs for one spawn request. Mirrors the fields the spawner exposed inline. */
	struct FAshSoldierSpawnParams
	{
		/** Unit-type tunables + visual/behavior sets. Null -> the Fallback* scalars below are used. */
		const UAshMassSoldierConfig* Config = nullptr;

		/** Team stamped on every entity. */
		EAshTeamId TeamId = EAshTeamId::Ally;

		/** Squad id stamped on every entity (FAshSquadFragment::SquadId). */
		int32 SquadId = 0;

		/** How many entities to create. Each block of FireteamSize entities forms one fireteam. */
		int32 Count = 0;

		/** Soldiers per fireteam (the V-formation cell). A "squad" of generals is a body of these. */
		int32 FireteamSize = 5;

		/** Centre of the deployment area (usually the spawner / general location). */
		FVector Origin = FVector::ZeroVector;

		/**
		 * Radius (cm) of the deployment area around Origin. Fireteams are distributed across this area
		 * (each at its own cluster) rather than every soldier scattered randomly, so squads start spread
		 * out instead of intermixed on one point.
		 */
		float SpawnRadius = 2000.f;

		/** Planar facing the V formations are oriented along at spawn (usually the general's forward). */
		FVector Forward = FVector::ForwardVector;

		// Inline fallbacks used only when Config is null (mirror UAshMassSoldierConfig defaults).
		float FallbackMaxHealth = 50.f;
		float FallbackMoveSpeed = 350.f;
		float FallbackAttackRange = 150.f;
		float FallbackAttackPower = 10.f;
		float FallbackAttackCooldown = 3.0f;
		float FallbackAttackCooldownVariance = 0.4f;
		float FallbackComboHitInterval = 0.45f;

		/**
		 * Morale-driven combo chances stamped onto every spawned soldier (Phase 29). Set by the owning
		 * general from its morale level; left 0 by the plain spawner (no general -> single hits only).
		 */
		float TwoHitChance = 0.f;
		float ThreeHitChance = 0.f;
	};

	/**
	 * Creates Params.Count soldier entities in World, seeds their fragments, and appends the new
	 * handles to OutEntities. Returns the number actually created (0 if Mass is unavailable or
	 * Count <= 0). The caller owns the returned handles' lifetime (destroy them on teardown).
	 *
	 * Note: this does NOT register a squad or draw debug — those are caller responsibilities, so the
	 * helper stays purely about entity creation.
	 */
	ASHDRAFTCORERUNTIME_API int32 SpawnSoldiers(
		UWorld* World,
		const FAshSoldierSpawnParams& Params,
		TArray<FMassEntityHandle>& OutEntities);
}
