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

		/** How many entities to create. */
		int32 Count = 0;

		/** Centre of the scatter disc (usually the spawner / general location). */
		FVector Origin = FVector::ZeroVector;

		/** Radius (cm) of the scatter disc around Origin. */
		float SpawnRadius = 2000.f;

		// Inline fallbacks used only when Config is null (mirror UAshMassSoldierConfig defaults).
		float FallbackMaxHealth = 50.f;
		float FallbackMoveSpeed = 350.f;
		float FallbackAttackRange = 150.f;
		float FallbackAttackPower = 10.f;
		float FallbackAttackCooldown = 1.5f;
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
