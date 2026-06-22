# DONE: Flow Field Navigation PoC (Phase 14)

## Summary

Implemented a flow-field group-navigation PoC for the Mass soldier army (ARCHITECTURE.md
12 / 12.1). Instead of every soldier pathfinding toward an objective, the battlefield is
quantised into a grid, a cost-to-goal (integration) field is solved once per target base,
and each cell's best direction is baked into a shared flow field. Mass soldiers read one
shared direction map, so a whole army advancing on the same base costs a single solve.

A config-backed switch on the movement processor (`GroupNavMode`) flips between the new
flow-field steering and the Phase 10 straight-line baseline, satisfying the "compare with
simple direct movement" task. A per-cell arrow debug view visualises the field.

## Files Changed

Created:
- `Source/AshDraftCoreRuntime/Public/Mass/AshFlowFieldConfig.h` — data-driven grid/cost
  tunables (`UAshFlowFieldConfig : UDataAsset`).
- `Source/AshDraftCoreRuntime/Public/Mass/AshFlowFieldSubsystem.h`
- `Source/AshDraftCoreRuntime/Private/Mass/AshFlowFieldSubsystem.cpp` —
  `UAshFlowFieldSubsystem : UWorldSubsystem`: grid bounds, cost field, per-cell ground-height
  sampling (`CellGroundZ`), Dijkstra integration field, baked flow field, lazy per-goal cache,
  direction query, and debug draw lifted above the sampled ground.

Modified:
- `Source/AshDraftCoreRuntime/Public/Mass/AshMassMovementProcessor.h` — added
  `EAshGroupNavMode`, `config=Game`, `GroupNavMode`, `bDrawFlowFieldDebug`.
- `Source/AshDraftCoreRuntime/Private/Mass/AshMassMovementProcessor.cpp` — group-objective
  steering now optionally reads the flow field; one-frame debug draw call.
- `Config/DefaultGame.ini` — `[/Script/AshDraftCoreRuntime.AshMassMovementProcessor]`
  (GroupNavMode=FlowField, bDrawFlowFieldDebug=True).
- `Docs/PLAN.md` — Phase 14 checked + summary.

Added guide:
- `Docs/Guides/Phase14_FlowField_Setup.md` — PIE verification + A/B comparison steps.

## Implementation Details

- **Grid (`EnsureGridInitialized`)**: lazily auto-bounds to the AABB of all registered
  bases (`UAshBaseSubsystem`) expanded by `GridMargin`; falls back to a box around origin
  when no bases exist. `CellSize` is grown automatically if the map would exceed
  `MaxCellsPerAxis`, keeping the solve bounded regardless of map scale. The grid plane Z is
  the average base height (debug-draw only).
- **Cost field**: open cells cost 1. With `bSampleObstacles`, one sphere overlap per cell
  against `WorldStatic` marks blocked cells impassable ("compute simple cell cost"). Off by
  default — the open PoC map has nothing to avoid and the overlap pass is the only heavy step.
- **Integration field (`BuildIntegrationField`)**: a label-correcting wavefront (SPFA)
  Dijkstra from the goal cell over 8 neighbours (orthogonal cost 1, diagonal √2), with
  corner-cutting prevention through blocked diagonals.
- **Flow field (`BuildFlowFromIntegration`)**: each passable cell points toward its
  lowest-integration neighbour (the gradient), normalised in 2D.
- **Caching**: keyed by goal *cell* index, so all soldiers attacking one base share a single
  solve and switching target bases is just a different key. Bases are static, so a cached
  field never goes stale; cache is bounded by `MaxCachedFields`.
- **Debug draw height (`CellGroundZ`)**: at grid init a single downward `ECC_WorldStatic` trace
  per cell records the surface height; arrows draw just above it. The grid plane itself is the
  average base height, which can fall below the terrain (it does on BaseLevel, floor at z≈−420),
  so drawing at the plane would bury the arrows. Cells whose trace misses fall back to the plane.
- **Movement integration**: only *group* objectives (squad objective / nearest capturable
  base) use the field; a combat target is always chased directly. Off-grid soldiers or the
  goal cell itself fall back to straight-line so progress is always made.

## Architecture Notes

- Data-oriented and shared, per ARCHITECTURE.md 7.4 / 12: "prefer shared movement fields,"
  no per-soldier pathfinding, no per-soldier Actor/Tick/BT.
- Passive subsystem solved lazily from the Mass movement processor — no Actor Tick
  (ARCHITECTURE.md 18.3). Soldiers hold no reference to the subsystem fragment-side; the
  processor brokers the query (18.4).
- Tunables are data-driven (`UAshFlowFieldConfig`) and the nav mode is config-backed so the
  Direct/FlowField comparison needs no rebuild (ARCHITECTURE.md 17).
- Soft dependency only on `UAshBaseSubsystem` for bounds; no hard hero/UI/AI coupling.

## Testing / Verification

- Build verified: `LyraEditor Win64 Development` — **Succeeded** (2026-06-22).
- Visual verified via unreal-mcp **Simulate** on BaseLevel (2026-06-22): with all soldiers on one
  team (so none acquire a combat target and all take the group-objective branch), the Mass army
  navigates the shared field and **cyan per-cell flow arrows render on the floor with the cubes
  gathering along them** toward the objective base. `GroupNavMode=FlowField` / `bDrawFlowFieldDebug=true`
  confirmed loading from `DefaultGame.ini` on the rebuilt binary (read off the processor CDO).
- Verification required a debug-draw fix (see below): without it the arrows were invisible because
  the grid plane (average base height) sits *below* the BaseLevel floor, so the arrows drew
  underground. Now each arrow is lifted above a per-cell ground-sampled height.

## Known Issues

- Obstacle sampling (`bSampleObstacles`) is a coarse one-probe-per-cell test and off by
  default; the PoC map is open. No dynamic obstacle updates (bases are static).
- `UAshFlowFieldConfig` is wired but no caller assigns one yet (`SetConfig` is unused), so the
  subsystem currently runs on inline defaults (CellSize 500, no obstacles). Assign one from a
  placed actor/subsystem init when tuning is needed.
- Cost is near-uniform; the field is essentially a shortest-path direction map, not yet a
  congestion/avoidance field (no soldier-density cost).
- Flow steering has no local separation, so soldiers can still bunch at the goal cell — finer
  spacing is the combat-slot system's job (ARCHITECTURE.md 10), not the flow field's.
- Debug-draw ground sampling relies on an `ECC_WorldStatic` floor; cells over a gap (or beyond
  the playable collision, as the grid margin can extend) fall back to the grid-plane height and
  may render their arrows underground. Cosmetic only — the simulation is unaffected.
- The flow path is only exercised when soldiers actually traverse toward a base. BaseLevel spawns
  both armies overlapping at the origin, so they melee immediately (combat target → direct
  steering) and the field is rarely queried; verification used a temporary one-team setup. The
  Phase 15 battlefield map (separated armies, real lanes) is the natural place this runs by default.

## Next Steps

- Phase 15: Battlefield PoC Map — author a proper battlefield with multiple bases and
  obstacles, which is also the ideal stage to validate `bSampleObstacles` routing.
