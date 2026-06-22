# DONE: Performance Validation (Phase 18)

> **Status: harness code-complete + build-verified (`LyraEditor Win64 Development` — Succeeded,
> 2026-06-22); measurements PENDING USER.** This phase is fundamentally a
> measurement pass that must run in PIE on real hardware. The code below makes the measurement
> repeatable and the LOD/time-slicing comparisons rebuild-free; the numbers tables are pre-staged
> for the user to fill in (see `Docs/Guides/Phase15-19_Editor_Work_And_Verification.md` §18).

## Summary

Added a performance-validation harness (ARCHITECTURE.md 7.5 / 9): runtime CVars that toggle AI LOD
and time slicing inside the Mass processors, plus console commands to set the soldier population
and dump a one-shot perf snapshot. This turns the Phase 18 checklist into a scripted procedure:
spawn N, read `stat unit`/`stat mass`, flip a CVar, compare.

## Files Changed

Created:
- `Source/AshDraftCoreRuntime/Public/Performance/AshPerfStatics.h` — `AshPerf::IsLODEnabled()` /
  `IsTimeSlicingEnabled()` accessors.
- `Source/AshDraftCoreRuntime/Private/Performance/AshPerfStatics.cpp` — CVar definitions,
  accessors, and the `Ash.Perf.*` console commands.

Modified:
- `Source/AshDraftCoreRuntime/Private/Mass/AshMassLODProcessor.cpp` — consumes the toggles: LOD off
  → every soldier forced to LOD 0 with `UpdateInterval=0` (combat runs full-rate for all);
  time slicing off → batch count collapses to 1 (whole population re-evaluated each frame).
- `Source/AshDraftCoreRuntime/Public/Mass/AshMassSoldierSpawner.h`
- `Source/AshDraftCoreRuntime/Private/Mass/AshMassSoldierSpawner.cpp` — public
  `SetDesiredSpawnCount` / `GetDesiredSpawnCount` / `Respawn` for the spawn command.
- `Docs/PLAN.md`, `Docs/Plan/Phase18.md`.

## Controls

| Control | Effect |
| --- | --- |
| `Ash.Perf.SpawnSoldiers <n>` | Set every `AAshMassSoldierSpawner` to `<n>` and respawn; logs total. |
| `Ash.Perf.Report` | Log + on-screen: FPS, Mass/actor soldier counts, active proxies, base counts, LOD/TimeSlice state. |
| `Ash.Mass.LOD 0\|1` | Disable/enable AI LOD (no rebuild). |
| `Ash.Mass.TimeSlice 0\|1` | Disable/enable LOD time slicing (no rebuild). |
| `stat unit` | FPS + Frame/Game/Draw/GPU ms (authoritative game-thread cost). |
| `stat mass` / `stat MassProcessor` | Mass processor cost breakdown. |

## Results — PENDING USER

Run on the Phase 15 battlefield (or BaseLevel) at a fixed camera. Fill in:

### Scaling (LOD + time slicing ON)

| Soldiers | FPS | Frame ms | Game ms | Mass ms | Active proxies |
| --- | --- | --- | --- | --- | --- |
| 100  |  |  |  |  |  |
| 300  |  |  |  |  |  |
| 500  |  |  |  |  |  |
| 1000 |  |  |  |  |  |

### AI LOD on/off (at 1000 soldiers)

| LOD | FPS | Game ms | Mass ms |
| --- | --- | --- | --- |
| ON (`Ash.Mass.LOD 1`)  |  |  |  |
| OFF (`Ash.Mass.LOD 0`) |  |  |  |

### Time slicing on/off (at 1000 soldiers)

| Time slicing | FPS | Game ms | Mass ms |
| --- | --- | --- | --- |
| ON (`Ash.Mass.TimeSlice 1`)  |  |  |  |
| OFF (`Ash.Mass.TimeSlice 0`) |  |  |  |

### Bottlenecks observed — PENDING USER

(e.g. combat-processor target acquisition grid, single-threaded game-thread execution, proxy
spawn cost, debug draw — note which dominates `stat mass`.)

## Architecture Notes

- Toggles are CVars, not config, so the on/off comparisons need no rebuild (ARCHITECTURE.md 9).
- The processors read `AshPerf::` accessors rather than the CVars directly, centralising defaults
  and CVar names.
- No new Tick; the harness drives the existing auto-registered Mass processors and spawners.

## Known Issues

- `Ash.Perf.Report` FPS is the last-frame instantaneous value; `stat unit` is the smoothed source
  of truth. Game-thread / Mass-processor cost still come from `stat unit` / `stat mass`, not the
  command (those numbers aren't cheaply readable from code).
- `Ash.Perf.SpawnSoldiers <n>` sets *each* spawner to `<n>` (so total = n × spawner count); divide
  if you want an exact global total, or place a single spawner for clean numbers.
- The combat processor's spatial grid is single-threaded (Phase 18 optimisation target, already
  flagged in its header) — expect it to dominate at high counts.

## Next Steps

- Phase 19 final review references these numbers for the "supports large-scale AI" conclusion.
- Future optimisation: parallelize the combat grid / move to a persistent spatial structure.
