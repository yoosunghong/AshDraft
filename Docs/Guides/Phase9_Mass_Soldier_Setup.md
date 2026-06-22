# Phase 9 Setup: Mass Entity Soldier Foundation

This guide walks you through building and verifying the Phase 9 Mass soldier foundation.
Everything was implemented in C++; you only need to build, place one actor, and run PIE.

## 1. Build

1. Close the editor (a plugin/module change requires a fresh build).
2. Regenerate project files (right-click `AshDraft.uproject` → *Generate Visual Studio
   project files*), or let the editor prompt you on next open.
3. Build the **AshDraft** target in your IDE (or open the `.uproject` and let it compile).

The build pulls in the Mass modules. Note the UE5.8 layout:

- `MassEntity` and `MassCore` are **built-in engine runtime modules** (no plugin to
  enable). `FMassEntityHandle` lives in `MassCore` (`Mass/EntityHandle.h`).
- `MassCommon`/`MassSpawner` come from the **Mass Gameplay** plugin, which is enabled in
  `AshDraft.uproject` and `AshDraftCore.uplugin`.
- `MassCore`, `MassEntity`, `MassCommon`, `MassSpawner` are public module dependencies in
  `AshDraftCoreRuntime.Build.cs`.

> Do **not** add a "MassEntity" entry to the Plugins list — there is no such plugin in
> UE5.8 and a non-existent plugin reference is a fatal error. Only **Mass Gameplay** needs
> enabling (Edit → Plugins → search "Mass").

This configuration has been build-verified (LyraEditor Win64 Development).

## 2. (Optional) Create a tuning config

1. Content Browser → right-click → **Miscellaneous → Data Asset**.
2. Pick **AshMassSoldierConfig** as the class.
3. Name it e.g. `DA_MassSoldierConfig` and set `MaxHealth`, `MoveSpeed`, `AttackRange`,
   `AttackPower`, `AttackCooldown`.

You can skip this — the spawner uses inline `Fallback*` values when no config is set.

## 3. Place the spawner

1. In your test map (e.g. `BaseLevel`), open the **Place Actors** panel.
2. Search for **AshMassSoldierSpawner** and drag one into the level on the NavMesh/ground.
3. In its Details panel:
   - `Config`: assign `DA_MassSoldierConfig` (optional).
   - `Team Id`: `Ally` or `Enemy` (changes the debug point color: Cyan vs Red).
   - `Spawn Count`: start at `300`. Bump to `1000`+ to stress the foundation.
   - `Spawn Radius`: area (cm) the soldiers scatter across.
   - `b Spawn On Begin Play`: leave checked.
   - `b Draw Debug`: leave checked so you can see the entities.

You can place a second spawner with `Team Id = Enemy` to see two armies.

## 4. Verify in PIE

1. Press **Play**.
2. **On screen:** a disc of colored debug points appears around the spawner — one per
   entity. Hundreds of points = hundreds of live Mass entities.
3. **In the Output Log** (Window → Output Log), filter for `LogAshMassSoldier`:
   ```
   AAshMassSoldierSpawner 'AshMassSoldierSpawner_0': created 300 Mass soldier entities (team 2, squad 0). Spawner now owns 300 entities.
   ```
4. **Cross-check with Mass tooling (optional):** open the console (`~`) and run
   `mass.debug` commands, or use **Tools → Audit → Mass** / the Gameplay Debugger
   (apostrophe key, Mass category) to inspect the entity/archetype counts.

### Scale check
Raise `Spawn Count` to 1000–5000 and confirm:
- The log reports the full count.
- PIE still starts quickly (entities are pure data — no Actors, no Tick, no Behavior
  Trees are created).

This proves the data-oriented goal: thousands of soldiers can exist far more cheaply
than thousands of `AAshSoldierCharacter` Actors.

## 5. What Phase 9 does *not* do

The entities don't move, fight, or render meshes yet — Phase 9 only establishes the
fragments, archetype, and spawning. Behavior comes from Mass Processors:

- **Phase 10:** movement + combat processors (entities advance and attack).
- **Phase 12:** LOD / time-slicing processor.
- **Phase 13:** representation (mesh / Actor proxy) so soldiers become visible bodies.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `UMassEntitySubsystem unavailable` error in log | Mass plugins not enabled — see step 1 note. |
| No debug points | `b Draw Debug` off, `Spawn Count` is 0, or the spawner is under the floor. |
| Build error on `GetMutableEntityManager` | Engine version mismatch — in some 5.x versions the accessor is `GetEntityManager()`; adjust in `AshMassSoldierSpawner.cpp`. |
| Build error on `CreateArchetype`/`CreateEntity` | Confirm `MassEntity` is in `Build.cs` and `#include "MassEntityManager.h"` resolves. |
