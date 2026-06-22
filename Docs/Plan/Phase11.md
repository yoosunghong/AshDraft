# Phase 11: Hierarchical AI Foundation

Goal: Implement the first Commander and Squad AI layer.

- [x] Create Commander subsystem or manager
  - `UAshCommanderSubsystem` (UWorldSubsystem). Binds once to
    `UAshBaseSubsystem::OnAnyBaseOwnershipChanged`; assigns orders on world begin-play, on
    squad registration, and on every base flip. No Tick.
- [x] Create Squad data model
  - `FAshSquadState` (+ `EAshSquadOrder`) in `AI/AshSquadTypes.h`; stored/served by
    `UAshSquadSubsystem`.
- [x] Add squad objective state
  - `EAshSquadOrder` + `ObjectiveLocation` + `bHasObjective`, set by the commander, read by
    `UAshMassMovementProcessor`.
- [x] Add squad average position tracking
  - `UAshMassSquadTrackingProcessor` aggregates living members' mean position per squad
    every `AggregateInterval` (0.5 s) into `UAshSquadSubsystem`.
- [x] Add squad alive count tracking
  - Same processor writes `AliveCount`.
- [x] Assign squads to bases
  - Commander: each squad → nearest base its team does NOT own (AttackBase); if its team
    owns all, defend nearest owned base.
- [x] Update squad orders when base ownership changes
  - `HandleBaseOwnershipChanged` → `ReevaluateOrders()`.
- [ ] Verify that soldiers follow squad-level goals
  - PENDING USER: compile + PIE. See `Docs/Guides/Phase10-13_Mass_AI_Setup.md`.
- [x] Create `Done/DONE_hierarchical_ai_foundation.md`

## Notes for compile / editor / verification (do later)
- One squad per spawner for now (the spawner's `SquadId`). Multiple spawners with distinct
  `SquadId`s give multiple squads; reused `SquadId`s merge.
- Initial squad centre-of-mass is seeded from the spawner location so the commander can
  plan before the first aggregation pass.
- Requires Phase 7 `AAshBaseActor`s present in the level for orders to be meaningful;
  with no bases, squads get `EAshSquadOrder::None` and fall back to combat-target / hold.
