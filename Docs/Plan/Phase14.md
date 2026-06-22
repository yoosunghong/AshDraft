# Phase 14: Flow Field Navigation PoC

Goal: Test group movement using a shared flow-field-like direction map.

- [x] Define flow field grid data
  - `UAshFlowFieldSubsystem` grid: origin/cell-size/cell-counts auto-bounded to the bases'
    AABB + margin, tunable via `UAshFlowFieldConfig`.
- [x] Define target base as flow field destination
  - Goal location resolved per soldier (squad objective / nearest capturable base) and fields
    are cached per goal *cell*, so the army sharing a base shares one solve.
- [x] Compute simple cell cost
  - Uniform cost 1 per open cell; optional one-probe-per-cell `ECC_WorldStatic` obstacle pass
    marks blocked cells impassable.
- [x] Compute best direction per cell
  - Dijkstra (SPFA wavefront) integration field from the goal cell, then each cell's flow =
    gradient toward its lowest-integration 8-neighbour.
- [x] Move Mass soldiers using flow direction
  - `UAshMassMovementProcessor` group-objective steering reads `GetFlowDirection` when
    `GroupNavMode == FlowField`; combat-target chasing stays direct.
- [x] Compare with simple direct movement
  - `GroupNavMode` (Direct / FlowField) is `config=Game`, flippable in `DefaultGame.ini` with
    no rebuild.
- [x] Add debug visualization
  - Per-cell cyan arrows + green goal sphere + black blocked points, lifted above a per-cell
    ground-sampled height (`CellGroundZ`) so they clear the floor. Gated by `bDrawFlowFieldDebug`.
- [x] Verify movement through a simple battlefield map
  - VERIFIED via unreal-mcp Simulate on BaseLevel (2026-06-22): cyan flow arrows render on the
    floor and the Mass cubes gather along them toward the objective base. See
    `Docs/Guides/Phase14_FlowField_Setup.md`.
- [x] Create `Done/DONE_flow_field_navigation_poc.md`
