# Phase 14 Setup: Flow Field Navigation PoC

This guide covers verifying the Phase 14 flow-field group navigation. The code is written,
**builds**, and was **visually verified via unreal-mcp Simulate** (2026-06-22): cyan per-cell
flow arrows render on the floor and the Mass cubes gather along them toward the objective base.

> Two gotchas this verification surfaced, baked into the notes below:
> 1. **Arrows draw above a per-cell ground-sampled height.** The grid plane is the average base
>    height, which on BaseLevel is *below* the floor (floor ≈ z−420); without the ground sample the
>    arrows draw underground and look absent.
> 2. **Both BaseLevel armies spawn overlapping at the origin**, so they melee immediately and the
>    flow field is rarely queried. To see it clearly, separate the armies or put everyone on one
>    team (no combat targets → all soldiers navigate to a base → field builds, arrows appear).

## 0. New / changed code at a glance

| Kind | Item |
| --- | --- |
| New data asset | `UAshFlowFieldConfig` — grid/cost tunables |
| New subsystem | `UAshFlowFieldSubsystem` — grid + integration field + flow field + cache + debug draw |
| Changed processor | `UAshMassMovementProcessor` — `GroupNavMode` (Direct/FlowField), `bDrawFlowFieldDebug` |
| Config | `Config/DefaultGame.ini` → `[/Script/AshDraftCoreRuntime.AshMassMovementProcessor]` |

No new module dependencies. The subsystem is a `UWorldSubsystem` (auto-created); the
processor is the existing auto-registered Mass processor. **Nothing new needs to be placed in
the level** — the flow field is built lazily the first time a soldier asks for a direction
toward a base.

## 1. Build

Already done this session (LyraEditor Win64 Development → Succeeded). If you pull fresh,
rebuild **AshDraft** (Win64 Development) with the editor closed.

## 2. Verify the flow field is built and followed

1. Open **BaseLevel** (the same map used to verify Phases 10–13: two `AshMassSoldierSpawner`s,
   Ally + Enemy, and at least one capturable/neutral base).
2. Confirm `Config/DefaultGame.ini` has:
   ```ini
   [/Script/AshDraftCoreRuntime.AshMassMovementProcessor]
   GroupNavMode=FlowField
   bDrawFlowFieldDebug=True
   ```
3. **Play (PIE)**. Expected:
   - A grid of **cyan per-cell arrows** appears, all pointing (cell by cell) toward a base.
   - A **green sphere** marks the goal cell (the target base each army is solving toward).
   - The Mass soldiers (their LOD debug points / proxy cubes) **flow along the arrows**
     toward the objective base rather than each taking a private straight line.
   - When a base flips ownership, the target base changes and a **new field** (new green
     sphere + re-pointed arrows) appears for the new goal cell.
4. If you enabled obstacle sampling (see §4) any blocked cells draw as **black points** and
   the arrows route around them.

> If no arrows appear: the flow field only builds once a soldier falls through to the
> "group objective / nearest base" steering branch. Make sure at least one army has no combat
> target yet (soldiers spawned away from the enemy) so they navigate toward a base.

## 3. A/B compare: Direct vs Flow Field (the "compare with direct movement" task)

1. Stop PIE. In `DefaultGame.ini` set `GroupNavMode=Direct`. Play.
   - Soldiers steer in straight lines at their nearest base; no arrows are drawn (debug draw
     is gated to FlowField mode). This is the Phase 10 baseline.
2. Stop PIE. Set `GroupNavMode=FlowField` again. Play.
   - Soldiers follow the shared field. On an open map the paths look similar; the win is that
     with obstacles (§4) the flow field routes *around* them while Direct walks into them, and
     the field is one shared solve regardless of army size.

(`GroupNavMode` is `config=Game`, so flipping it needs no rebuild — just restart PIE.)

## 4. Optional: prove obstacle routing

Obstacle sampling is data-driven and off by default. To try it you must assign a config,
because nothing assigns one automatically yet (`UAshFlowFieldSubsystem::SetConfig` is unused):

- Easiest path is to add the obstacle flag/grid tuning by extending the subsystem to load a
  config (e.g. from a placed actor's `BeginPlay`, or a project setting), then create a
  `UAshFlowFieldConfig` data asset under `/AshDraftCore/...`, set `bSampleObstacles=true`, and
  place some `WorldStatic` blockers between an army and its target base. Blocked cells draw as
  black points and arrows bend around them.
- This is genuinely Phase 15 (Battlefield PoC Map) territory; on the current open BaseLevel
  there is nothing to route around, so §2–§3 are the meaningful Phase 14 checks.

## 5. Turn off the debug draw for normal play

Set `bDrawFlowFieldDebug=False` in `DefaultGame.ini` once verified — the arrows are a
verification aid, not gameplay.

## Troubleshooting

| Symptom | Likely cause / fix |
| --- | --- |
| No arrows at all | No soldier reached the group-objective branch (all had combat targets), or `GroupNavMode=Direct`. Spawn an army away from the enemy. |
| Arrows but soldiers ignore them | Mass simulation not ticking (see Phase 10–13 guide §2) — the movement processor isn't running. |
| Arrows point the wrong way | Goal cell resolved to a friendly base; check the army's team vs base ownership (`FindNearestCapturableBase` skips own-team bases). |
| Huge/sparse grid | `MaxCellsPerAxis` forced `CellSize` to grow because the bases' AABB + margin is large. Lower `GridMargin` or raise `MaxCellsPerAxis` (needs a config wired, §4). |
