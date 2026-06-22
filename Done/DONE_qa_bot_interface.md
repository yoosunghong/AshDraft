# DONE: Telemetry and QA Bot Interface Preparation (Phase 17)

## Summary

Implemented the future QA-bot / reinforcement-learning interface (ARCHITECTURE.md 14): a single
world-scoped telemetry hub plus the flat data contracts an external agent round-trips. It covers
the three pieces of the doc's `Reset()/Step()/GetObservation()` shape:

- **Observation** (`FAshObservation`, 14.1) — built on demand from the live world.
- **Action** (`FAshAction`, 14.2) — applied back to the hero (the `Step(Action)` seam).
- **Logging + export** (14.4) — combat / base-event / match-result logs serialized to JSON.

The subsystem only *observes* gameplay; it never drives AI or owns UI (16/18.4), and holds no hard
unit references — it resolves the hero/soldiers/bases through existing subsystems each call.

## Files Changed

Created:
- `Source/AshDraftCoreRuntime/Public/QA/AshQABotTypes.h` — `FAshObservation`, `FAshAction`,
  `FAshCombatLogEntry`, `FAshBaseEventLogEntry`, `FAshMatchResultLogEntry` (all `USTRUCT`/
  `BlueprintType`, UPROPERTY fields for JSON).
- `Source/AshDraftCoreRuntime/Public/QA/AshTelemetrySubsystem.h`
- `Source/AshDraftCoreRuntime/Private/QA/AshTelemetrySubsystem.cpp` —
  `UAshTelemetrySubsystem : UWorldSubsystem`.

Modified:
- `Source/AshDraftCoreRuntime/Private/AbilitySystem/AshAttributeSet.cpp` — combat-log hook at the
  damage choke point in `PostGameplayEffectExecute` (soft, null-guarded `UAshTelemetrySubsystem::Get`).
- `Source/AshDraftCoreRuntime/AshDraftCoreRuntime.Build.cs` — `+ "Json", "JsonUtilities"`.
- `Docs/PLAN.md`, `Docs/Plan/Phase17.md`.

## Implementation Details

- **BuildObservation()** gathers: player health/max/alive/position/yaw (from the hero), nearby
  enemy count (`CountNearbyEnemies` — living enemy actor soldiers + generals within
  `ObservationRadius`), base counts (owned = Player+Ally / enemy / neutral via `UAshBaseSubsystem`),
  a current-objective string (`ComputeCurrentObjective` → nearest non-player-side base as
  `Capture:<name>`, else `Defend:<name>`), and the match state.
- **ApplyAction()** is a real abstraction, not a stub: it applies the camera-yaw delta to the
  controller, screen-relative movement via `AddMovementInput` (mirroring the hero's `Input_Move`),
  and routes attack/skill/dodge through `UAshAbilitySystemComponent::AbilityInputTagPressed` using
  the same input tags Enhanced Input uses. Guarded on a live hero.
- **Logging** uses capped ring buffers (`MaxLogEntries`, oldest trimmed). The base-event and
  match-result logs auto-subscribe (`OnAnyBaseOwnershipChanged`, `OnMatchEnded`) so they need no
  explicit calls. The combat log is fed from the single authoritative GAS damage path so every
  hero/general hit is captured with source/target/amount and a `Damage`/`Kill` classifier.
- **JSON export** uses `FJsonObjectConverter` to convert the observation + each log array into a
  top-level object (`observation`, `combat_log`, `base_event_log`, `match_result_log`).
  `ExportToFile("")` writes a timestamped file under `<ProjectSaved>/AshTelemetry/` and logs the path.

## Architecture Notes

- Observe-only (ARCHITECTURE.md 16): the subsystem reads state through other subsystems and the
  player controller; the only inbound dependency is the GAS damage path calling the static `Get()`.
- No Tick (18.3); observations are pull-based and logs are event-driven.
- Data-oriented contracts: flat `USTRUCT`s mirroring the doc's JSON so a Python/RL harness consumes
  them directly.

## Testing / Verification

- **Build verified**: `LyraEditor Win64 Development` — **Succeeded** (2026-06-22), including the
  new `Json`/`JsonUtilities` dependency and the `AshAttributeSet.cpp` combat hook, no errors.
- PIE smoke test (run a match, check Output-Log for the export path, open the JSON) is PENDING USER —
  `Docs/Guides/Phase15-19_Editor_Work_And_Verification.md` §17 has the diagnostic checklist
  (subsystem present → observation fields sane → logs populate → file written/parses).

## Known Issues

- **Nearby-enemy count is actor-based only** — Mass-entity soldiers (the bulk of the army) are not
  yet counted; this matches the project's current actor-registry seam (`UAshSoldierSubsystem`). A
  Mass spatial query is the future extension.
- No console command for export yet (call `ExportToFile` from BP/an actor, or add one later); the
  match-result + base logs do persist across the match automatically.
- `ApplyAction` is wired but nothing calls it yet — it is the seam for a future scripted/RL driver.
- Combat log captures GAS (hero/general) damage only; high-frequency Mass-soldier combat is
  deliberately excluded to avoid flooding (would need sampling).

## Next Steps

- Phase 18 measurement can reuse the report path; Phase 19 review confirms interface readiness.
- Future: a `Ash.QA.Dump` console command, a Mass-aware nearby-enemy query, and a driver that calls
  `BuildObservation`→policy→`ApplyAction` each step.
