# Phase 12: AI LOD and Time Slicing

Goal: Reduce simulation cost by updating units based on importance.

- [x] Create AI LOD calculation model
  - `UAshMassLODProcessor` → `FAshLODFragment.LODLevel` (0..3).
- [x] Use distance to player
  - LOD from distance between the entity and the local player pawn (resolved softly via
    `GetFirstPlayerController()->GetPawn()`; no hard hero dependency).
- [ ] Use camera visibility placeholder
  - PARTIAL: stubbed. Currently distance-only; camera-visibility factor not yet applied.
- [ ] Use objective importance
  - PARTIAL: stubbed. NearBase/NearHero/RecentlyDamaged/objective-importance factors
    (ARCHITECTURE.md 9.2) not yet folded in; distance-only for now.
- [x] Add update interval per LOD level
  - `LODUpdateIntervals[4]` (0.05 / 0.15 / 0.5 / 1.0 s) → `FAshLODFragment.UpdateInterval`,
    consumed by `UAshMassCombatProcessor`.
- [x] Add time-sliced update batches
  - LOD recompute split into `NumTimeSliceBatches` (default 4); one batch per frame
    (ARCHITECTURE.md 9.4).
- [x] Apply lower frequency updates to far soldiers
  - Combat processor skips an entity until `WorldTime - LastUpdateTime >= UpdateInterval`.
- [x] Add debug display for LOD level
  - Per-frame LOD-coloured point per soldier (green/yellow/orange/red) when `bDrawLODDebug`.
- [ ] Compare performance before and after LOD
  - PENDING USER: needs PIE + `stat unit` / `stat mass`. See guide.
- [x] Promote LOD tunables to a `UAshMassLODConfig` data asset (for RL / automated perf sweeps)
  - `UAshMassLODConfig` (thresholds + `LODUpdateIntervals[4]` + `NumTimeSliceBatches`);
    `UAshMassLODProcessor` resolves it once on first Execute via a config-persisted
    `TSoftObjectPtr` (config=Game), falling back to inline ctor defaults when unassigned.
- [x] Create `Done/DONE_ai_lod_time_slicing.md`

## Notes for compile / editor / verification (do later)
- The combat processor is the single owner of `FAshLODFragment.LastUpdateTime` (the only
  LOD-throttled heavy processor), avoiding multi-writer conflicts on that field.
- Movement integration intentionally runs every frame regardless of LOD (cheap), so far
  soldiers still glide smoothly; only decision-making is throttled.
- KNOWN LIMITATION to revisit (Phase 12 polish / 18): camera-visibility and importance LOD
  inputs are stubbed. Flagged here so it is not mistaken for a bug during verification.
