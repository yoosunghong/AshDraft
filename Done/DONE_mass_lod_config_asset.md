# DONE: UAshMassLODConfig Data Asset (Phase 12 follow-up)

> Status: CODE COMPLETE — NOT yet compiled or run (this is a standalone plugin repo with no
> `.uproject`; build happens from the parent Lyra project's editor target, as in prior phases).

## Summary

Promoted the AI-LOD tunables off the `UAshMassLODProcessor` CDO and onto a data-driven
`UAshMassLODConfig` asset, so RL / automated perf runs can sweep LOD aggressiveness (distance
rings, per-LOD update cadence, time-slice batch count) without recompiling or hand-editing the
processor defaults. Behaviour is unchanged until a config is assigned — the processor still
falls back to its inline ctor defaults.

## Files Changed

Created:
- `Public/Mass/AshMassLODConfig.h` — `UAshMassLODConfig : UDataAsset` (BlueprintType), mirroring
  the existing `UAshMassSoldierConfig` / `UAshFlowFieldConfig` pattern.

Modified:
- `Public/Mass/AshMassLODProcessor.h` — added a config-persisted `TSoftObjectPtr<UAshMassLODConfig>
  LODConfig`, a `ResolveConfig()` helper, a `bConfigResolved` guard, and changed `UCLASS()` →
  `UCLASS(config = Game)` so the asset assignment persists to the parent project's `DefaultGame.ini`
  (same reason the Phase 13 representation processor is `config=Game`).
- `Private/Mass/AshMassLODProcessor.cpp` — `ResolveConfig()` (synchronous load once on first
  `Execute`, copies the asset's values into the working fields); called at the top of `Execute`.
- `Docs/PLAN.md`, `Docs/Plan/Phase12.md`.

## Implementation Details

- **Asset shape.** `LOD0/1/2MaxDistance`, `LODUpdateIntervals[4]`, `NumTimeSliceBatches` — the
  exact set of tunables the processor previously hard-defaulted in its ctor, with the same clamps
  and defaults (0.05 / 0.15 / 0.5 / 1.0 s; 4 batches).
- **Resolution model.** The processor keeps its inline fields as the live working values /
  fallback. `ResolveConfig()` runs once (guarded by `bConfigResolved`), `LoadSynchronous()`-es the
  soft pointer, and overwrites the working fields when an asset is present. Synchronous load is
  acceptable: it happens a single time and the asset is tiny. No per-frame indirection is added to
  the hot loop — `Execute` reads the same plain floats as before.
- **Persistence.** The asset reference is stored as a `TSoftObjectPtr` with the `config` specifier;
  `UCLASS(config=Game)` writes it to `[/Script/AshDraftCoreRuntime.AshMassLODProcessor]` in the
  project `DefaultGame.ini`. A non-config native CDO has no on-disk persistence, so without this the
  assignment would reset every editor launch (lesson carried over from Phase 13).

## Architecture Notes

- Follows ARCHITECTURE.md 17 / 7.4 (data-asset-driven tuning, "avoid hardcoded gameplay numbers")
  and matches the established `UAshMassSoldierConfig` / `UAshFlowFieldConfig` convention: a
  `BlueprintType UDataAsset` consumed by its system with an inline-default fallback.
- Orthogonal to the Phase 18 runtime CVars (`Ash.Mass.LOD` / `Ash.Mass.TimeSlice`): those toggle
  the system on/off live; this asset tunes the *values* used when it is on.

## Testing / Verification (PENDING USER)

1. Build from the parent project editor target (LyraEditor Win64), as in prior phases.
2. In the editor, create a `UAshMassLODConfig` asset (right-click → Miscellaneous → Data Asset →
   `AshMassLODConfig`), e.g. `/AshDraftCore/Game/Mass/DA_MassLODConfig`.
3. Assign it to the processor: Project Settings has no page for plain `UMassProcessor` CDOs, so set
   it in `DefaultGame.ini` under `[/Script/AshDraftCoreRuntime.AshMassLODProcessor]`,
   `LODConfig=/AshDraftCore/Game/Mass/DA_MassLODConfig.DA_MassLODConfig` (or edit the CDO via the
   class-defaults editor if exposed), then restart.
4. Perf sweep: duplicate the asset, set all four `LODUpdateIntervals` to 0 (full-rate baseline) in
   one and keep defaults in the other, swap the `DefaultGame.ini` reference between runs, and
   compare `stat unit` / `stat mass` at 1000+ soldiers with `bDrawLODDebug` off.

## Known Issues

- Not compiled here (no `.uproject` in this plugin repo).
- Assigning the asset is `.ini`/CDO-level rather than a placed-actor property (intrinsic to LOD
  living on a `UMassProcessor` CDO, not an actor). A future option is a project-settings developer
  page, but that is more than the perf-sweep use case needs.
- Distance-only LOD inputs (camera visibility / objective importance) remain stubbed — unchanged by
  this work; see `DONE_ai_lod_time_slicing.md`.

## Next Steps

Optionally fold the Phase 18 CVar toggles and this config into a single `UAshMassLODConfig` (add
`bLODEnabled` / `bTimeSlicingEnabled` defaults) so one asset is the full LOD control surface for RL
runs. Then drive automated sweeps by swapping the assigned asset between episodes.
