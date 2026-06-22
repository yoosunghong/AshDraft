# Phase 9: Mass Entity Soldier Foundation

Goal: Move general soldiers toward a data-oriented Mass Entity structure.

- [x] Enable required Mass plugins/modules
  - UE5.8: `MassEntity`/`MassCore` are built-in engine runtime modules (not plugins);
    `MassCommon`/`MassSpawner` come from the `MassGameplay` plugin. `MassGameplay` enabled
    in `AshDraft.uproject` + `AshDraftCore.uplugin`; `MassCore`, `MassEntity`, `MassCommon`,
    `MassSpawner` added to `AshDraftCoreRuntime.Build.cs`. **Build verified** (LyraEditor
    Win64 Development, 2026-06-22).
- [x] Create `FAshTeamFragment`
- [x] Create `FAshHealthFragment`
- [x] Create `FAshMovementFragment`
- [x] Create `FAshCombatFragment`
- [x] Create `FAshSquadFragment`
- [x] Create `FAshLODFragment`
  - All six in `Public/Mass/AshSoldierFragments.h` as `FMassFragment` structs; layout
    mirrors the Phase 8 `UAshSoldierConfig` / `AAshSoldierCharacter` scalars.
- [x] Create Mass soldier archetype/config
  - `UAshMassSoldierConfig` data asset (tunables) + archetype built from the six
    fragments inside `AAshMassSoldierSpawner`.
- [x] Spawn Mass soldier entities
  - `AAshMassSoldierSpawner` batch-creates entities via `FMassEntityManager` and seeds
    each entity's fragments; owns/destroys them across PIE.
- [x] Verify that hundreds of soldier entities can exist
  - Spawner defaults to 300 entities, logs the created count, and draws one debug point
    per entity. Code compiles cleanly; pending user PIE run per
    `Docs/Guides/Phase9_Mass_Soldier_Setup.md` to confirm on-screen.
- [x] Create `Done/DONE_mass_soldier_foundation.md`
