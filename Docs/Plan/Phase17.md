# Phase 17: Telemetry and QA Bot Interface Preparation

Goal: Prepare future reinforcement-learning or scripted QA bot integration.

- [x] Define Observation data structure — `FAshObservation` (`AshQABotTypes.h`).
- [x] Define Action data structure — `FAshAction` (`AshQABotTypes.h`).
- [x] Add player state observation — health/max/alive/position/yaw in `BuildObservation`.
- [x] Add nearby enemy count observation — `CountNearbyEnemies` (actor soldiers + generals in radius).
- [x] Add base ownership observation — owned/enemy/neutral base counts via `UAshBaseSubsystem`.
- [x] Add current objective observation — `ComputeCurrentObjective` ("Capture:<base>" / "Defend:<base>").
- [x] Add action input abstraction — `UAshTelemetrySubsystem::ApplyAction` (move + camera + ability tags).
- [x] Add combat log — `LogCombatEvent`, fed from `UAshAttributeSet::PostGameplayEffectExecute`.
- [x] Add base event log — auto-subscribed to `OnAnyBaseOwnershipChanged`.
- [x] Add match result log — auto-subscribed to `UAshMatchSubsystem::OnMatchEnded`.
- [x] Export observation/log data as JSON — `GetObservationJson` / `ExportToJson` / `ExportToFile`.
- [x] Create `Done/DONE_qa_bot_interface.md`

> All code-complete; build + a PIE smoke test (confirm the JSON file + fields) is PENDING USER —
> see `Docs/Guides/Phase15-19_Editor_Work_And_Verification.md` §17. Known limit: Mass-entity
> soldiers are not yet included in the nearby-enemy count (actor-registry seam only).