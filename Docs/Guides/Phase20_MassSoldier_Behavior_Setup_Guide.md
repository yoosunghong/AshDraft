# Phase 20 — Mass Soldier Behavior / Collision Setup & Verification Guide

This phase added the soldier **local-AI state machine** and fixed the burial / facing / shaking
defects. The C++ is done; this guide covers the small amount of editor data work and how to verify.

## 0. Build first

The build must be done with the editor closed (or via Live Coding), because the editor locks the
module binaries:

- Editor closed: run
  `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" LyraEditor Win64 Development -Project="C:\Users\Foryoucom\Documents\GitHub\AshDraft\AshDraft.uproject" -WaitMutex`
- Editor open: press **Ctrl+Alt+F11** to trigger a Live Coding compile.

## 1. Create a behavior data asset per unit type

1. Content Browser → right-click → **Miscellaneous → Data Asset** → pick **AshSoldierBehaviorConfig**.
2. Name it e.g. `DA_Infantry_Behavior` (put it next to your other soldier data assets, e.g.
   `Content/Data/Soldier/`).
3. Tune the fields (defaults are sensible; these are the ones that matter most):

   | Field | Default | What it does |
   |---|---|---|
   | `LocalSenseRadius` | 600 | How close an enemy must be before the soldier engages. Keep small — soldiers are local. |
   | `MaxLeashFromObjective` | 1200 | How far from the squad objective a soldier may chase before returning. 0 = no leash. |
   | `TurnRateDegPerSec` | 720 | How fast the body turns to face its target/travel. |
   | `SeparationRadius` | 90 | Personal space (≈ body diameter). |
   | `SeparationStrength` | 0.6 | Push strength vs. move speed. |
   | `SeparationRelaxation` | 0.5 | **Lower = calmer crowd.** Primary anti-shake knob. |
   | `MaxPushPerNeighbor` | 1.0 | Caps a single neighbour's shove (prevents flinging). |
   | `CombatAnchorResistance` | 0.85 | How hard an attacking soldier resists being pushed. Raise toward 1 if the line still drifts. |
   | `bConformToGround` | true | Snap the soldier's height to the terrain. |
   | `GroundTraceChannel` | WorldStatic | Channel your landscape/ground blocks. Set to Visibility if your ground only blocks Visibility. |

## 2. Link it from the unit-type config

1. Open your `DA_MassSoldierConfig` (and the `_Ally` / `_Enemy` variants).
2. Set the new **Behavior** field (category `Ash|MassSoldier|Behavior`) to the `DA_*_Behavior` asset.
3. (Leave `Visual` as you already have it.)

> Soldiers with no Behavior assigned still work — the processors use built-in fallbacks — but you
> won't get per-unit tuning or ground conform.

## 3. Fix the per-mesh vertical offset (the other half of "buried")

Ground conform fixes the *world* height; the *local* pelvis→feet alignment is still per skeletal mesh:

1. Open your `DA_<Unit>_Visual` (UAshSoldierVisualConfig).
2. Open the skeletal mesh in the Skeletal Mesh editor and read how far the mesh origin sits above the
   feet (often ~88–90 cm for UE-standard rigs, but measure yours).
3. Set `MeshRelativeLocation.Z` to the negative of that (e.g. `-88`). If soldiers still sink or float
   on flat ground, nudge this value — it is the art correction, not the world height.

## 4. Verify in PIE

Spawn two opposing armies (your spawners / `Ash.Perf.SpawnSoldiers`) and check, ideally on terrain
with a slope:

1. **Burial** — soldiers stand on the ground on both flat and sloped areas. If half-buried on flat
   ground → adjust `MeshRelativeLocation.Z` (step 3). If buried only on slopes → `bConformToGround`
   is off, the Behavior asset isn't linked, or `GroundTraceChannel` doesn't match your ground.
2. **Facing** — when two soldiers fight, each faces its enemy while striking (not sideways/backwards).
   If they face their travel direction instead, confirm the build picked up the proxy/movement change.
3. **No shaking / no shoving** — gather 5+ soldiers; the cluster settles to stable spacing without
   vibrating. In a lopsided fight (e.g. 30 vs 10), the larger side should **not** push the smaller
   side's line backward while attacking. If it still drifts, raise `CombatAnchorResistance` toward
   `1.0` and/or lower `SeparationRelaxation`.

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Soldiers never engage | `LocalSenseRadius` too small, or armies are hostile-misconfigured (teams), or Behavior unset (fallback sense is 600). |
| Soldiers chase enemies across the map | `MaxLeashFromObjective` too large or 0; lower it so they hold the squad line. |
| Soldiers float above ground | `MeshRelativeLocation.Z` too negative, or trace hitting an invisible volume; check the trace channel. |
| Still buried on slopes | Behavior asset not linked on `DA_MassSoldierConfig.Behavior`, or `bConformToGround` false, or wrong `GroundTraceChannel`. |
| Crowd still jitters | Lower `SeparationRelaxation` (e.g. 0.35) and/or `SeparationStrength`. |
| Front line gets pushed back | Raise `CombatAnchorResistance` toward 1.0. |
