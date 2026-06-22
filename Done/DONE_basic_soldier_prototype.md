# DONE: Basic Soldier Prototype (Phase 8)

## Summary

Implemented the temporary Actor-based soldier (ARCHITECTURE.md 6.3 / PLAN Phase 8) that
validates small-scale soldier combat before the Mass Entity implementation. A soldier has
a data-driven archetype, team identity, authoritative health, NavMesh movement toward an
objective, nearest-hostile target acquisition with a simple melee attack on cooldown, and
event-driven death. A world subsystem holds the soldier registry and answers the "nearest
enemy" / "team count" queries so soldiers don't reference each other directly.

Crucially this is a *throwaway* representation: every tunable field maps onto the future
`FAsh*Fragment` layout (7.2) so Phases 9–10 can port the data-oriented version mechanically.

## Files Changed

C++ (`Source/AshDraftCoreRuntime/`), all new:
- `Public/Soldier/AshSoldierConfig.h` — `UAshSoldierConfig` data asset (the soldier data model).
- `Public/Soldier/AshSoldierSubsystem.h` + `Private/Soldier/AshSoldierSubsystem.cpp` —
  `UAshSoldierSubsystem` world registry + nearest-hostile / team-count queries.
- `Public/Soldier/AshSoldierCharacter.h` + `Private/Soldier/AshSoldierCharacter.cpp` —
  `AAshSoldierCharacter` prototype soldier.

Docs:
- `Docs/Plan/Phase8.md`, `Docs/PLAN.md` — checkboxes updated.
- `Docs/Guides/Phase8_Soldier_Setup.md` — asset authoring + PIE verification guide.

No `Build.cs` change: `AIModule` / `NavigationSystem` / `GameplayTags` were already present.

## Implementation Details

- **Data model (`UAshSoldierConfig`):** MaxHealth, MoveSpeed, AttackRange, AttackDamage,
  AttackInterval, DetectionRadius, ThinkInterval, MoveAcceptanceRadius, DeathLifeSpan. The
  soldier resolves each value from config with an inline `Fallback*` when none is assigned
  (same pattern as `AAshBaseActor`).
- **No Tick, no BT (18.3 / 7.4):** behavior runs in `Think()` on a repeating timer at
  `ThinkInterval` (default 5 Hz, which doubles as the per-unit AI update interval, 9.3). The
  first tick is staggered by a random sub-interval so a spawned wave doesn't act in lockstep.
  The think loop is a handful of local rules: acquire target → if in range stop+attack, else
  move toward target, else advance to the objective.
- **Movement:** a plain `AAIController` (auto-possessed) provides NavMesh path-following via
  `MoveToActor` / `MoveToLocation`. Move requests are debounced against the active goal so
  following a moving target doesn't spam the path-follow component. `SetObjectiveLocation`
  gives squad/commander layers a hook; default objective is the spawn location.
- **Team identity:** implements `IAshTeamAgentInterface`; hostility resolved through
  `UAshTeamStatics::AreTeamsHostile` so soldiers classify the same way as every other unit.
- **Health & attack:** authoritative scalar health (not GAS — soldiers become Mass entities
  with a health fragment, not ASC actors). `ReceiveDamage(Amount, Instigator)` is the single
  authority-guarded entry point (5.5 / 15). `TryAttack` faces the target, gates on
  `AttackInterval` via world time, and calls the target's `ReceiveDamage`.
- **Death (event-driven):** at zero health `HandleDeath` clears the think timer, stops the
  controller + movement, drops collision, **drives the Phase 7 capture loop** by calling
  `NotifyDefenderDied` on the nearest base owned by its team (11.2), unregisters from the
  subsystem (stops being targeted immediately), broadcasts `OnSoldierDied`, then despawns
  after `DeathLifeSpan`.
- **Subsystem:** weak-pointer registry mirroring `UAshBaseSubsystem`; `FindNearestHostileSoldier`
  skips dead/stale/friendly entries and respects a max distance.

## Architecture Notes

- Temporary Actor soldier exactly as 6.3 prescribes ("nearby important soldiers may
  temporarily use Actor proxies"); the data layout is pre-aligned to the Mass fragments so
  the migration is a port, not a redesign (7.2 / 7.4).
- Dependency rules honored (18.4): soldier→soldier and soldier→base coordination both go
  through subsystems / interfaces, never hard class references; no UI or AI coupling.
- Authority-friendly: damage/death are server-authoritative and event-driven, not cosmetic
  local effects (5.5 / 15). Closes the loop with Phase 7 — defender deaths now actually
  drain base durability.
- Tunables are data-driven (17); nothing gameplay-relevant is hardcoded on the actor.

## Testing / Verification

C++ complete; not yet run (MCP editor bridge unavailable this session). The user runs the
C++ build + PIE verification following `Docs/Guides/Phase8_Soldier_Setup.md`: place ally and
enemy soldiers within detection range on the Phase 5 NavMesh and confirm move → acquire →
attack → die, plus the optional base-durability drain on defender death.

## Known Issues

- Soldiers only damage **other soldiers** this phase; they do not yet damage the GAS-based
  hero / enemy general (cross-damage to ASC actors is a later integration).
- Each soldier path-finds individually — acceptable at small scale and the explicit reason
  this is a prototype; Phases 9–10 (Mass + flow field) replace it with shared movement.
- No spawning/squad system yet: soldiers are placed by hand (or via a level script). Base
  `DefenderRespawnDelay` is still a hook, not wired to spawn soldiers.
- Health is a raw scalar with no hit reaction / GAS effects (intentional for the Mass port).

## Next Steps

- Phase 9: Mass Entity Soldier Foundation — define `FAshTeamFragment` / `FAshHealthFragment`
  / `FAshMovementFragment` / `FAshCombatFragment` etc. and port this prototype's data and
  rules onto Mass entities + processors, retiring per-soldier Actor Tick-equivalents and
  individual pathfinding.
