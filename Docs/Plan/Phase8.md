# Phase 8: Basic Soldier Prototype

Goal: Create a temporary soldier representation before full Mass integration.

- [x] Create basic soldier data model
  - `UAshSoldierConfig` data asset (health/movement/combat/AI tunables), fields mapped to
    the future `FAsh*Fragment` layout.
- [x] Create temporary Actor-based soldier or debug representation
  - `AAshSoldierCharacter` (ACharacter), timer-driven think loop (no Tick, no per-soldier BT);
    `UAshSoldierSubsystem` world registry for target/team queries.
- [x] Add team identity
  - `EAshTeamId` + `IAshTeamAgentInterface`; resolved by `UAshTeamStatics` like other units.
- [x] Add health
  - Authoritative scalar health on the soldier (not GAS), `ReceiveDamage` entry point.
- [x] Add simple movement toward objective
  - `SetObjectiveLocation` + AIController NavMesh `MoveToLocation`; defaults to spawn location.
- [x] Add simple attack behavior
  - Acquire nearest hostile via subsystem, advance, `MoveToActor`, melee at `AttackRange` on cooldown.
- [x] Add death handling
  - Health-zero → stop brain/movement, disable collision, notify nearest owned base
    (`NotifyDefenderDied`), broadcast `OnSoldierDied`, despawn after `DeathLifeSpan`.
- [x] Verify small-scale soldier combat
  - C++ complete. Pending user C++ build + PIE run with the authoring steps in
    `Docs/Guides/Phase8_Soldier_Setup.md` (2026-06-22).
- [x] Create `Done/DONE_basic_soldier_prototype.md`
