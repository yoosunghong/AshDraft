# DONE: Mass Movement and Combat Processors (Phase 10)

> Status: CODE COMPLETE — NOT yet compiled, run, or verified in editor (the user asked to
> exclude compile/editor/PIE this pass). See "Testing / Verification" and "Known Issues"
> for everything that must be done later, all at once.

## Summary

Added the first behavioural Mass Processors that bring the Phase 9 soldier entities to
life: they advance toward an objective, acquire nearby enemies, trade simple damage, and
are removed when killed — all data-oriented, batched, game-thread, no Actor Tick / no
per-soldier Behavior Tree (ARCHITECTURE.md 7.3 / 7.4 / 8.3 / 18.3).

## Files Changed

Created:
- `Public/Mass/AshMassMovementProcessor.h` / `Private/Mass/AshMassMovementProcessor.cpp`
- `Public/Mass/AshMassCombatProcessor.h` / `Private/Mass/AshMassCombatProcessor.cpp`
- `Public/Mass/AshMassDeathProcessor.h` / `Private/Mass/AshMassDeathProcessor.cpp`

Modified:
- `Private/Mass/AshMassSoldierSpawner.cpp` — registers the spawner's squad with the squad
  subsystem + notifies the commander (shared with Phase 11).
- `Docs/PLAN.md`, `Docs/Plan/Phase10.md`.

(Depends on the Phase 11 `UAshSquadSubsystem`/`UAshBaseSubsystem` for objective lookup; the
movement processor degrades gracefully if those are absent.)

## Implementation Details

- **UE5.8 processor idiom.** Each processor is a `UMassProcessor` with a member
  `FMassEntityQuery EntityQuery` initialised `: EntityQuery(*this)`, overrides
  `ConfigureQueries(const TSharedRef<FMassEntityManager>&)` to declare fragment
  requirements, and `Execute(...)` that calls `EntityQuery.ForEachEntityChunk(Context, ...)`
  iterating with `FMassExecutionContext::FEntityIterator`. All set
  `ExecutionFlags = AllNetModes`, `ProcessingPhase = PrePhysics`,
  `bRequiresGameThreadExecution = true`, and auto-register with the processing phases.
- **Movement.** Reads `FAshMovementFragment` (RW) + `FAshCombatFragment`/`FAshSquadFragment`/
  `FAshTeamFragment` (RO). Destination priority: valid combat target position → squad
  objective (`UAshSquadSubsystem::GetSquadObjective`) → nearest base not owned by the
  soldier's team (`UAshBaseSubsystem`). Sets velocity toward the destination (stopping
  inside attack range of a target) and integrates `Position += Velocity * dt` every frame.
- **Combat.** Two-pass: (A) snapshot all soldiers into a file-local uniform spatial grid
  (`FAshCombatGrid`, cell = `AcquisitionRadius`); (B) per soldier, throttled by the AI LOD
  interval, acquire the nearest hostile living entry in the 3×3 cell neighbourhood, store it
  in `FAshCombatFragment::Target`, and if in `AttackRange` and off cooldown, subtract
  `AttackPower` from the target's `FAshHealthFragment` (written via
  `EntityManager.GetFragmentDataPtr`). Hostility via `UAshTeamStatics::AreTeamsHostile`.
- **Death.** Scans `FAshHealthFragment`; at `CurrentHealth <= 0` queues
  `Context.Defer().DestroyEntity(...)`. Ordered `ExecuteAfter` combat so same-frame kills are
  reaped. Destroying the entity invalidates its handle, so other soldiers' cached targets
  drop naturally next combat tick.

## Architecture Notes

- Cheap-every-frame integration vs throttled-decisions split matches ARCHITECTURE.md 9.3:
  movement integrates each frame; expensive target acquisition is LOD-gated (Phase 12).
- No hard hero/UI references; cross-system data via subsystems and team statics (18.4).
- Damage is applied from an authoritative game-thread context, not a cosmetic local event
  (ARCHITECTURE.md 5.5 / 15).

## Testing / Verification (PENDING USER — do later, all at once)

1. **Build** `LyraEditor Win64 Development`.
2. **VERIFY PROCESSORS RUN FIRST.** Auto-registered processors only execute if the Mass
   simulation is ticking. In PIE with an `AshMassSoldierSpawner` placed, soldiers should
   move. If they are completely inert, the scope of the problem is "Mass processing phases
   not running in this world" (check `mass.SimulationTickingEnabled`, that the MassGameplay
   plugin's `UMassSimulationSubsystem` is active) — NOT the processor logic.
3. Place two spawners, Team **Ally** vs Team **Enemy**, overlapping (spawn radius 2000,
   combat `AcquisitionRadius` 1500). Play: armies should converge, fight, and thin out.
4. Watch `LogAshMassDeath` (Verbose) for reaped counts and confirm entity count drops.

## Known Issues / Scope notes

- **NOT COMPILED.** Written to the UE5.8 Mass API as read from engine source; first build
  may surface signature nits — if so, that is the immediate scope (API mismatch), not design.
- Combat grid is rebuilt single-threaded each pass (O(n) but game-thread). Fine for the
  PoC; a Phase 18 optimization target for very large counts.
- Death does not yet drain bases (the Phase 8 actor soldier did via `NotifyDefenderDied`).
  Base interplay for Mass soldiers is deferred — flagged, not forgotten.
- No ragdoll/FX on death; entity simply disappears (representation is Phase 13).

## Next Steps

Phase 11 (hierarchical AI) is implemented alongside this; after a successful build, verify
the Phase 10 battle, then the Phase 11 squad-following behaviour.
