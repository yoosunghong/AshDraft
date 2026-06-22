# DONE: AI LOD and Time Slicing (Phase 12)

> Status: CODE COMPLETE — NOT yet compiled, run, or perf-compared (excluded this pass). See
> "Testing / Verification" and "Known Issues".

## Summary

Added `UAshMassLODProcessor`, which classifies each soldier into an LOD level from its
distance to the player and writes a per-level update interval that the combat processor uses
to throttle expensive work — so far-away soldiers simulate less often
(ARCHITECTURE.md 9). The LOD recompute is itself time-sliced across frames, and an optional
debug pass draws LOD-coloured points (which also visualises Phase 10 movement).

## Files Changed

Created:
- `Public/Mass/AshMassLODProcessor.h` / `Private/Mass/AshMassLODProcessor.cpp`.

Modified (Phase 10, the consumer):
- `Private/Mass/AshMassCombatProcessor.cpp` already reads `FAshLODFragment.UpdateInterval` /
  `LastUpdateTime` to gate its per-soldier work.
- `Docs/PLAN.md`, `Docs/Plan/Phase12.md`.

## Implementation Details

- **LOD model.** Distance from each entity's `FAshMovementFragment.Position` to the local
  player pawn (resolved softly via `GetFirstPlayerController()->GetPawn()` — no hard hero
  dependency, ARCHITECTURE.md 18.4) maps to LOD 0..3 via `LOD0/1/2MaxDistance` thresholds.
  No player → farthest LOD.
- **Update intervals.** `LODUpdateIntervals[4]` (0.05 / 0.15 / 0.5 / 1.0 s) → written into
  `FAshLODFragment.UpdateInterval`. The combat processor (single owner of `LastUpdateTime`)
  skips a soldier until `WorldTime - LastUpdateTime >= UpdateInterval`, giving the per-LOD
  frequencies of ARCHITECTURE.md 9.3.
- **Time slicing.** A frame counter selects one of `NumTimeSliceBatches` (default 4); only
  entities whose running index `% NumBatches == activeBatch` are re-evaluated that frame
  (ARCHITECTURE.md 9.4). LOD changes slowly, so this is invisible but cuts recompute cost.
- **Debug display.** When `bDrawLODDebug`, one point per soldier each frame coloured
  green/yellow/orange/red by LOD level. Doubles as the movement visualisation until Phase 13
  proxies render meshes.

## Architecture Notes

- Implements "important things precise, large-scale things cheap" (ARCHITECTURE.md 20):
  near soldiers fight at up to 20 Hz, far soldiers at ~1 Hz, with smooth movement preserved
  by the cheap every-frame integration in the movement processor.
- Single-writer discipline on `FAshLODFragment.LastUpdateTime` (combat only) prevents
  multi-processor contention on that field.

## Testing / Verification (PENDING USER — do later)

1. Build + PIE with a large `AshMassSoldierSpawner` (e.g. 1000–3000) near the player.
2. Confirm LOD colours form rings around the player (green near → red far) and shift as the
   player moves.
3. **Perf compare** (the unchecked plan item): with `stat unit` / `stat mass`, compare frame
   time with `bDrawLODDebug` off and large counts, vs a build where combat is forced every
   frame (e.g. temporarily set all `LODUpdateIntervals` to 0). Expect lower game-thread cost
   with LOD throttling on.

## Known Issues / Scope notes

- **NOT COMPILED** — possible Mass-API nits (see Phase 10 doc).
- **Distance-only LOD.** Camera-visibility and objective-importance / NearBase / NearHero /
  RecentlyDamaged factors (ARCHITECTURE.md 9.2) are STUBBED. This is a deliberate limitation,
  not a bug — do not treat "a soldier behind the camera is still LOD 0 if close" as a defect.
- LOD thresholds/intervals live on the processor CDO (config) rather than a `UDataAsset`;
  could be promoted to a data asset later for designer tuning.
- Debug draw cost is O(n) per frame; turn `bDrawLODDebug` off for perf measurement.

## Next Steps

Fold camera frustum + importance inputs into the LOD score; consider a dedicated
`UAshMassLODConfig` data asset; use LOD level to drive Phase 13 proxy promotion (already
wired) and future animation LODs.
