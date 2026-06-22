# DONE: Mass Entity Soldier Foundation

## Summary

Established the data-oriented Mass Entity foundation for general soldiers (Phase 9).
Enabled the Mass plugins/modules, defined the six soldier data fragments, added a
data-driven Mass soldier config, and built an archetype + spawner that batch-creates and
seeds hundreds (or thousands) of soldier entities through the `FMassEntityManager` — with
no per-soldier Actor, Tick, or Behavior Tree. This replaces the Phase 8 prototype
`AAshSoldierCharacter` as the scalable representation; behavior processors follow in
Phase 10.

## Files Changed

Created:
- `Source/AshDraftCoreRuntime/Public/Mass/AshSoldierFragments.h` — the six
  `FMassFragment` structs.
- `Source/AshDraftCoreRuntime/Public/Mass/AshMassSoldierConfig.h` — tunable archetype
  data asset.
- `Source/AshDraftCoreRuntime/Public/Mass/AshMassSoldierSpawner.h`
- `Source/AshDraftCoreRuntime/Private/Mass/AshMassSoldierSpawner.cpp` — archetype build +
  entity batch-create/seed/destroy.
- `Docs/Guides/Phase9_Mass_Soldier_Setup.md` — build + PIE verification guide.

Modified:
- `Source/AshDraftCoreRuntime/AshDraftCoreRuntime.Build.cs` — added `MassCore`,
  `MassEntity`, `MassCommon`, `MassSpawner` modules.
- `Plugins/GameFeatures/AshDraftCore/AshDraftCore.uplugin` — added the `MassGameplay`
  plugin dependency.
- `AshDraft.uproject` — enabled the `MassGameplay` plugin (and fixed a stray trailing
  comma in the Plugins array).
- `Docs/PLAN.md`, `Docs/Plan/Phase9.md` — checkboxes + completion notes.

### UE5.8 module layout (important)
`MassEntity` is **not a plugin** in UE5.8 — it is a built-in engine *runtime module*
(`Engine/Source/Runtime/MassEntity`), and `FMassEntityHandle` lives in the new
**MassCore** module at `Mass/EntityHandle.h`. Only `MassCommon`/`MassSpawner` (inside the
`MassGameplay` plugin) require a plugin to be enabled. Listing a non-existent "MassEntity"
plugin in the `.uproject`/`.uplugin` is fatal — that, plus the missing `MassCore`
dependency/include, was the original compile failure; both are fixed.

## Implementation Details

- **Fragments (ARCHITECTURE.md 7.2).** All six fragments derive from `FMassFragment` and
  are grouped in one header because they are trivial POD data definitions with no `.cpp`
  (the one-class-per-file rule targets substantial classes). Field set deliberately
  mirrors the Phase 8 `UAshSoldierConfig` / `AAshSoldierCharacter` scalars so the port is
  mechanical:
  - `FAshTeamFragment { EAshTeamId TeamId }`
  - `FAshHealthFragment { CurrentHealth, MaxHealth }`
  - `FAshMovementFragment { Position, Velocity, MoveSpeed }`
  - `FAshCombatFragment { FMassEntityHandle Target, AttackRange, AttackCooldown, AttackPower, TimeSinceLastAttack }`
  - `FAshSquadFragment { SquadId, OrderId }`
  - `FAshLODFragment { LODLevel, UpdateInterval, LastUpdateTime }`
  The combat target is a real `FMassEntityHandle` (the doc's conceptual "TargetEntityId").
- **Config.** `UAshMassSoldierConfig` (a `UDataAsset`) holds the health/movement/combat
  tunables, keeping values out of code (17 / 7.4). The Mass version drops Actor-only
  fields (think interval, nav acceptance) that have no meaning for a fragment-only soldier.
- **Archetype + spawning.** `AAshMassSoldierSpawner` gets the world's
  `UMassEntitySubsystem`, builds an archetype from the six fragment `StaticStruct()`s via
  `FMassEntityManager::CreateArchetype`, then creates `SpawnCount` entities, seeding each
  one's fragments from the config (or inline fallbacks) and scattering positions across a
  disc. The spawner owns the entity handles and destroys them on `EndPlay` for clean PIE
  restarts.
- **Verification hooks.** Logs the created/owned entity count under `LogAshMassSoldier`
  and draws one debug point per entity (team-colored) so hundreds of entities are visible
  on screen even though no representation processor exists yet.

## Architecture Notes

- **Data-oriented, no Actor sprawl (7.1 / 7.4 / 7.5).** Soldiers are now entity IDs +
  compact fragments instead of `ACharacter`s. No per-soldier Actor, Tick, Behavior Tree,
  or individual pathfinding is created — exactly the cost model the architecture calls for
  at scale.
- **Tick policy (18.3).** The spawner has `bCanEverTick = false`; future per-frame work
  belongs in Mass Processors, not Actor Tick.
- **Data Assets for tuning (17).** All gameplay numbers live in `UAshMassSoldierConfig`,
  with inline fallbacks only as a convenience default.
- **Team model reuse (18.1).** Fragments reuse the existing `EAshTeamId`, so Mass soldiers
  share the project's team vocabulary with heroes/general/bases.
- **Processor seam (7.3).** Fragments and the archetype are shaped for the upcoming
  `UAshMassMovementProcessor` / `UAshMassCombatProcessor` / `UAshMassLODProcessor` /
  `UAshMassRepresentationProcessor` (Phases 10/12/13) to query and mutate.

## Testing / Verification

Build verified here: `LyraEditor Win64 Development` compiles and links cleanly
(`Result: Succeeded`). Runtime PIE verification is pending the user, per
`Docs/Guides/Phase9_Mass_Soldier_Setup.md`:
1. Place an `AshMassSoldierSpawner` in a level; set `Spawn Count` (default 300).
2. Play: confirm the debug-point disc and the `LogAshMassSoldier` "created N entities" log.
3. Scale `Spawn Count` to 1000–5000 to confirm hundreds/thousands of entities exist with
   fast PIE startup.

## Known Issues

- **No behavior yet.** Entities are inert data — they do not move, fight, or render. That
  is by design for the foundation; processors arrive in Phase 10+.
- **No representation.** Soldiers are only visible via debug points until the
  representation processor (Phase 13) gives them meshes/proxies.
- **Per-entity create loop.** `CreateEntity` is called per soldier rather than via a
  batch-create API. Fine for the foundation's scale; revisit during performance
  validation (Phase 18) if spawn cost matters at very large counts.
- **Unused module deps.** `MassCommon`/`MassSpawner` are linked but not yet used by Phase 9
  code; they are kept for the Phase 10 processors and are harmless (build is green).
- **Two parallel soldier systems.** The Phase 8 `AAshSoldierCharacter` still exists; it is
  intentionally retained as the near-range/high-fidelity path and will be reconciled with
  the hybrid representation work (Phase 13).

## Next Steps

Phase 10: implement the Mass Processors (`UAshMassMovementProcessor`,
`UAshMassCombatProcessor`, and supporting `UAshMassDeathProcessor`) that consume these
fragments to make the entities advance toward an objective and fight, replacing the Phase
8 actor think-loop with batched data-oriented processing.
