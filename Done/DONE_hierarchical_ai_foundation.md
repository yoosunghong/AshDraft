# DONE: Hierarchical AI Foundation (Phase 11)

> Status: CODE COMPLETE — NOT yet compiled or verified in PIE (excluded this pass). See
> "Testing / Verification" and "Known Issues".

## Summary

Added the first two layers of the hierarchical AI above the data-oriented soldiers
(ARCHITECTURE.md 8): a **Commander** that turns base ownership into strategic squad orders,
and a **Squad** model that holds each group's shared objective and aggregate stats. Soldiers
stay dumb — they only carry a `SquadId` and follow the shared objective the movement
processor reads (ARCHITECTURE.md 8.3 / 7.4).

## Files Changed

Created:
- `Public/AI/AshSquadTypes.h` — `EAshSquadOrder` + `FAshSquadState`.
- `Public/AI/AshSquadSubsystem.h` / `Private/AI/AshSquadSubsystem.cpp` — squad registry,
  objectives, aggregates.
- `Public/AI/AshCommanderSubsystem.h` / `Private/AI/AshCommanderSubsystem.cpp` — strategic
  order assignment reacting to base ownership.
- `Public/Mass/AshMassSquadTrackingProcessor.h` /
  `Private/Mass/AshMassSquadTrackingProcessor.cpp` — aggregates avg position + alive count.

Modified:
- `Private/Mass/AshMassSoldierSpawner.cpp` — registers its squad + seeds centre-of-mass +
  triggers `ReevaluateOrders()`.
- `Private/Mass/AshMassMovementProcessor.cpp` — reads `UAshSquadSubsystem::GetSquadObjective`.
- `Docs/PLAN.md`, `Docs/Plan/Phase11.md`.

## Implementation Details

- **Squad data model.** `FAshSquadState { SquadId, TeamId, Order, ObjectiveLocation,
  bHasObjective, AveragePosition, AliveCount }`. `EAshSquadOrder { None, AttackBase,
  DefendBase, Retreat }` mirrors ARCHITECTURE.md 8.1's order vocabulary.
- **UAshSquadSubsystem (UWorldSubsystem).** Single source of truth for squad state, keyed by
  id. `RegisterSquad`, `SetSquadOrder`, `GetSquadObjective`, `GetSquadState`,
  `UpdateSquadAggregate`, plus id enumeration. Soldiers reference squads only by int id
  (ARCHITECTURE.md 18.4 — no hard refs).
- **UAshCommanderSubsystem (UWorldSubsystem).** On `OnWorldBeginPlay` it binds to
  `UAshBaseSubsystem::OnAnyBaseOwnershipChanged` and calls `ReevaluateOrders()`. Strategy:
  for each squad, find the nearest base its team does not own → `AttackBase` at that base;
  if its team owns all bases → `DefendBase` at the nearest owned base; none → `None`/hold.
  Re-runs on every base flip and on squad registration. No Tick.
- **UAshMassSquadTrackingProcessor.** Every `AggregateInterval` (0.5 s) sums living members'
  positions per `SquadId` and writes the mean + count back via `UpdateSquadAggregate` — the
  data-oriented → strategic feedback path (ARCHITECTURE.md 8.2).

## Architecture Notes

- Clean three-tier flow: Commander (orders) → Squad subsystem (shared objective) → Mass
  movement processor (soldier execution) → Squad tracking processor (stats back up). Matches
  ARCHITECTURE.md 8's `Commander ↓ Squad ↓ Soldier` hierarchy.
- Event-driven (base-ownership delegate), not polled (ARCHITECTURE.md 18.3).

## Testing / Verification (PENDING USER — do later)

1. Build, then PIE with ≥1 `AshBaseActor` and two `AshMassSoldierSpawner`s on opposing
   teams with distinct `SquadId`s.
2. Confirm each squad advances on the nearest enemy/neutral base (its assigned objective).
3. Force a base capture (Phase 7 flow) and confirm squads re-target (commander reacts to the
   ownership-change delegate).
4. Optionally inspect squad state via Blueprint `GetSquadState` (avg position / alive count
   should update ~2 Hz).

## Known Issues / Scope notes

- **NOT COMPILED** — see Phase 10 doc's caveat about possible Mass-API signature nits.
- One squad per spawner; no squad splitting/merging or formation keeping yet
  (ARCHITECTURE.md 8.2 "maintain formation" is future work).
- Commander strategy is intentionally minimal (nearest-base attack/defend); no
  reinforcement-of-hero, retreat thresholds, or army-level controller layer yet.
- Squad objective uses the base's actor location; no per-soldier formation offset (soldiers
  converge on the same point — fine for the PoC, refine with Flow Field in Phase 14).

## Next Steps

Phase 14 Flow Field navigation (group movement toward squad objectives without per-soldier
pathing); later, formation offsets and an army/team controller layer.
