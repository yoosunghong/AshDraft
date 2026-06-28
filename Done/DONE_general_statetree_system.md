# DONE: General StateTree System (Phase 22)

## Summary

Added the **operational AI layer** that was missing between the strategic commander and the
data-oriented soldiers: the **General**. A General is a high-fidelity, team-agnostic `ACharacter`
(not a Mass entity) driven by an Unreal **StateTree**. It takes the commander's strategic order,
executes it on the ground — temporarily preempting it to react to higher-priority threats it meets
in its path, then resuming — and commands a data-driven body of Mass soldiers that follow it.

This unifies and replaces the Phase 5 Behavior-Tree enemy general (same GAS foundation and
event-driven death), as requested.

Control flow now completes the hierarchy:

```
Commander (which base)        UAshCommanderSubsystem
   ↓ order + objective
General (StateTree)           AAshGeneralCharacter / AAshGeneralController   ← NEW (actor AI-LOD)
   ↓ squad objective = general's live position (+ FormationRadius)
Mass soldiers (local rules)   existing follow / engage / return behavior
```

## Files Changed

**New**
- `Public/AI/AshGeneralTypes.h` — `FAshGeneralState` (registry record) + `EAshThreatType`.
- `Public/AI/AshGeneralConfig.h` — `UAshGeneralConfig` data asset (troops, radii, StateTree, LOD).
- `Public/AI/AshGeneralSubsystem.h` + `Private/AI/AshGeneralSubsystem.cpp` — general registry,
  commander seam, and the actor-AI-LOD think-rate driver (timer, no Tick).
- `Public/Character/AshGeneralCharacter.h` + `Private/Character/AshGeneralCharacter.cpp` — the General.
- `Public/AI/AshGeneralController.h` + `Private/AI/AshGeneralController.cpp` — AIController hosting
  `UStateTreeAIComponent`.
- `Public/AI/StateTree/AshGeneralStateTreeNodes.h` + `Private/AI/StateTree/AshGeneralStateTreeNodes.cpp`
  — `FAshSTTask_ExecuteOrder`, `FAshSTTask_AttackTarget`, `FAshSTTask_AssaultStronghold`,
  `FAshSTCondition_HasThreat`.
- `Public/Mass/AshMassSoldierSpawnLibrary.h` + `Private/Mass/AshMassSoldierSpawnLibrary.cpp` — shared
  soldier archetype build + fragment seeding.

**Modified**
- `Private/Mass/AshMassSoldierSpawner.cpp` — now spawns via the shared library (behavior unchanged).
- `Private/AI/AshCommanderSubsystem.cpp` — assigns orders to generals; skips general-owned squads;
  shared `ComputeOrderForPosition` helper used for both squads and generals.
- `Public/AI/AshSquadTypes.h` — `FAshSquadState::FormationRadius`.
- `Public/AI/AshSquadSubsystem.h` + `.cpp` — `SetSquadObjective(...)` + a `GetSquadObjective` overload
  returning the formation radius.
- `Private/Mass/AshMassMovementProcessor.cpp` — group-objective arrival honors `FormationRadius`.
- `AshDraftCoreRuntime.Build.cs` — `StateTreeModule`, `GameplayStateTreeModule`.
- `AshGameplayTags.h/.cpp` — `State.Commanding`, `Objective.Engage`, `Objective.Assault`.

## Implementation Details

- **Troops are data-driven and general-owned.** `UAshGeneralConfig::TroopCount` soldiers of
  `SoldierConfig`'s unit type are spawned on BeginPlay into a squad the general owns (squad ids start at
  1000 to avoid colliding with placed-spawner ids). The general registers that squad and itself, then
  asks the commander to (re)plan. Troop entities are destroyed on EndPlay so PIE restarts cleanly.
- **Following / formation.** Every StateTree task republishes the squad objective = the general's own
  location each tick (with the config `FormationRadius`). The soldiers' existing Phase 20/20.1 behavior
  (follow squad objective, engage within `LocalSenseRadius`, leash from engage anchor, return) then
  keeps them as a body around the general: they trail it as it moves and ring it (at `FormationRadius`,
  via the movement processor's arrival change) when it holds, while still being free to peel off briefly
  to fight and then rejoin.
- **Priority / sub-objectives without complex editor wiring.** The three task states are siblings
  ordered AttackTarget > AssaultStronghold > ExecuteOrder, each with a `HasThreat` enter condition
  (ExecuteOrder has none). The lower-priority tasks *self-yield* (return `Failed`) the instant a
  higher-priority premise appears, so the parent re-selects the first sibling whose condition passes.
  That produces "commander order is the base intent; an enemy or a stronghold in the path preempts it;
  resume afterward" with only ordering + a single condition per state in the asset.
- **Threat sensing** is cheap and on the general: nearest hostile actor (player + registered generals)
  within `EnemyEngageRadius`, nearest hostile base within `StrongholdDetourRadius`. The cache refreshes
  no more than once per think interval, so it is naturally throttled by AI-LOD.
- **Actor AI-LOD.** `UAshGeneralSubsystem` reclassifies every general by distance to the player from one
  repeating timer and calls `AAshGeneralCharacter::UpdateThinkLOD`, which sets both the StateTree
  component tick interval and the sensing cadence (near ≈ 30–60 Hz, far ≈ 1–2 Hz). No per-general Tick.

## Architecture Notes

- Mirrors the existing squad/commander seam (ARCHITECTURE.md 8, 18.4): the general holds an id and a
  squad id; soldiers never hard-reference the general — they read a shared objective by squad id.
- No Actor Tick anywhere (CLAUDE.md 18.3): StateTree-, timer-, GAS- and delegate-driven.
- All tunables are data (CLAUDE.md 17): troop count, radii, attack range, LOD rings + think intervals,
  StateTree asset — all on `UAshGeneralConfig`.
- Reuses, not reinvents: the Mass spawn path (shared library), the soldier follow/engage behavior, the
  GAS basic-attack input-tag activation (same path as the hero / Phase 5 BT), the commander's
  nearest-base rule, and the LOD ring concept.
- The commander change is additive — general-less placed-spawner squads behave exactly as before, so
  prior phases keep working.

## Testing / Verification

- **Build:** LyraEditor Win64 Development with the **editor closed** (new structs/UPROPERTYs are not
  Live-Coding-safe).
- **Editor authoring** (see `Docs/Guides/Phase22_General_StateTree_Setup_Guide.md`): create
  `ST_AshGeneral` (StateTree AI Component schema) wiring the four C++ nodes; create `DA_General_*`;
  re-parent the enemy general BP to `AAshGeneralCharacter` with `AAshGeneralController`; place general(s).
- **PIE:** TroopCount soldiers spawn and ring the general when it idles; the general marches to its
  commander objective with the troops following; an enemy dropped in its path makes it preempt to
  AttackTarget (troops converge) and resume after the enemy dies; an enemy base in the corridor triggers
  AssaultStronghold then resume; moving a general far from the player drops its think-rate; the
  re-parented enemy general still fights the hero (Phase 5 parity, now StateTree-driven).

## Known Issues

- Threat sensing covers actor combatants (player + generals) and bases, not individual Mass soldiers
  (they have no actors). Soldier-vs-general melee is the existing proxy-hit path; a general does not
  currently retarget onto a single Mass soldier. Acceptable for the PoC; revisit if generals need to
  brawl crowds directly.
- On a general's death the troops persist leaderless holding their last objective (no rally/hand-off
  yet). A future "reassign squad to nearest friendly general / commander-direct" step is the natural
  follow-up.
- The StateTree asset itself is authored in-editor (MCP cannot create StateTree assets); until
  `ST_AshGeneral` + `DA_General_*` exist and the BP is re-parented, the system is inert at runtime.

## Next Steps

- Author the StateTree + data assets and re-parent the enemy general; PIE-verify and fill results here.
- Add an **ally general** (Team = Ally) so the player fights alongside a commanded body.
- Leaderless-troop rally on general death; optional general retreat/regroup using the morale scalar.
