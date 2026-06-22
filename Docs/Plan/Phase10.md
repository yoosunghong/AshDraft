# Phase 10: Mass Movement and Combat Processors

Goal: Implement basic data-oriented soldier simulation.

- [x] Create `UAshMassMovementProcessor`
- [x] Create `UAshMassCombatProcessor`
- [x] Create `UAshMassDeathProcessor`
- [x] Move soldiers toward squad/base objective
  - Priority: valid combat target → squad objective (`UAshSquadSubsystem`) → nearest
    non-friendly base (`UAshBaseSubsystem`) fallback. Position is integrated every frame
    from velocity (cheap); decisions are LOD-throttled.
- [x] Detect nearby enemy soldiers in simplified form
  - Coarse uniform spatial grid (cell = `AcquisitionRadius`, 3×3 neighbourhood) rebuilt
    each combat pass; ~O(n) instead of O(n²). Hostility via `UAshTeamStatics`.
- [x] Apply simple damage
  - `AttackPower` subtracted from the target's `FAshHealthFragment` when in `AttackRange`
    and off cooldown (`TimeSinceLastAttack >= AttackCooldown`).
- [x] Remove or mark dead soldiers
  - `UAshMassDeathProcessor` queues `Context.Defer().DestroyEntity` at 0 HP (deferred so the
    structural change is safe). Invalidated handles auto-clear stale combat targets.
- [ ] Verify simple Mass soldier battle
  - PENDING USER: needs compile + editor + PIE (excluded from this pass per request).
    See `Docs/Guides/Phase10-13_Mass_AI_Setup.md`. **Verify first:** that the auto-
    registered Mass processors actually run (Mass simulation must be ticking). If soldiers
    are inert, that is the scope of the problem — not the processor logic.
- [x] Create `Done/DONE_mass_movement_combat.md`

## Notes for compile / editor / verification (do later, all at once)
- New files compile-only so far; **not yet built**. Build `LyraEditor Win64 Development`.
- No new module dependency was needed (processors only need `MassEntity`, already linked).
- Place two `AshMassSoldierSpawner`s (Team Ally vs Team Enemy) close enough that their
  spawn discs (radius 2000) and the combat `AcquisitionRadius` (1500) overlap, else they
  never engage.
- Movement is visualised by the Phase 12 LOD processor's per-frame debug points; the
  Phase 9 spawner's one-shot 60 s debug draw does not show motion (can disable its
  `bDrawDebug`).
