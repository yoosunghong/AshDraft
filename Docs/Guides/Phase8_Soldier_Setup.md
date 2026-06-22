# Phase 8 — Soldier prototype setup & verification (manual editor step)

The Phase 8 soldier is fully implemented in C++. The MCP editor bridge was not connected
this session, so the asset authoring + PIE verification below must be done by hand in the
editor after a C++ build.

## 0. Build

Build the `AshDraftCoreRuntime` module (Live Coding or a full editor build). New classes
that must appear afterward:

- `AshSoldierConfig` (Data Asset)
- `AshSoldierCharacter` (Character)
- `AshSoldierSubsystem` (auto-created world subsystem, no asset)

## 1. Soldier config data assets

Content Browser → `/AshDraftCore/Soldier` (create the folder) →
**Add → Miscellaneous → Data Asset → `AshSoldierConfig`**.

Make two so the two sides differ a little (optional — defaults are fine):

| Asset                | MaxHealth | MoveSpeed | AttackRange | AttackDamage | AttackInterval | DetectionRadius |
| -------------------- | --------- | --------- | ----------- | ------------ | -------------- | --------------- |
| `DA_Soldier_Ally`    | 50        | 350       | 150         | 10           | 1.5            | 1500            |
| `DA_Soldier_Enemy`   | 50        | 350       | 150         | 10           | 1.5            | 1500            |

(Leave `ThinkInterval = 0.2`, `MoveAcceptanceRadius = 80`, `DeathLifeSpan = 3`.)

## 2. Soldier Blueprint

`/AshDraftCore/Game/Soldier` → **Blueprint Class → `AshSoldierCharacter`**, name it
`B_Soldier`. In Class Defaults / components:

- Assign a visible mesh + AnimBP on the inherited `Mesh` (any humanoid/capsule placeholder;
  the prototype is functional, cosmetics are optional).
- `Config` — leave **None** on the base BP; set it per-instance (or make two child BPs
  `B_Soldier_Ally` / `B_Soldier_Enemy` each with its config + `TeamId`).
- `bShowDebug` = **true** while verifying (draws `HP x/y [engaging]` above each soldier).

Recommended: two child BPs, or just set `TeamId` + `Config` on each placed instance.

## 3. Place a skirmish in `BaseLevel`

The level already has a `NavMeshBoundsVolume` + `RecastNavMesh` from Phase 5 (needed for
soldier `MoveTo`). Confirm the soldiers are inside the nav bounds (press **P** to view nav).

- Place ~4 soldiers with `TeamId = Ally` on one side.
- Place ~4 soldiers with `TeamId = Enemy` ~1000–1500 cm away (within `DetectionRadius`).
- Make sure `AutoPossessAI = Placed in World or Spawned` (default on the class).

Optional objective: to make them advance even before they see each other, call
`SetObjectiveLocation` (e.g. from a Level Blueprint BeginPlay, pass the enemy muster point).
Without it, soldiers hold their spawn and engage once an enemy enters detection range.

## 4. Verify (PIE)

Press Play and confirm:

1. **Movement** — soldiers path over the NavMesh toward each other / the objective.
2. **Target acquisition** — each soldier picks the nearest hostile (debug shows `[engaging]`).
3. **Attack** — at ~150 cm they stop, face the target, and the target's HP ticks down every
   `AttackInterval`.
4. **Death** — a soldier at 0 HP stops, drops collision, disappears after `DeathLifeSpan`,
   and is no longer targeted.
5. **Base hook (optional)** — if an `AAshBaseActor` owned by the dying soldier's team is
   nearby, its durability drops by `DurabilityPerDefenderDeath` on each defender death
   (enable the base's `bShowDebug` to watch).

Expected end state: one team is wiped out; survivors return to holding their objective.

## Notes / known limits

- Soldiers fight **other soldiers** only in this prototype. They do not yet damage the
  hero/enemy general (those are GAS actors); cross-damage is a later integration.
- Each soldier path-follows individually — fine at this small scale, and exactly what the
  Mass migration (Phases 9–10) replaces with shared flow-field movement.
- If soldiers refuse to move, the cause is almost always missing/!built NavMesh — rebuild
  paths or extend the `NavMeshBoundsVolume`.
