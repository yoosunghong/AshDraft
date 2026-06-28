# Phase 22 — General StateTree: Editor Setup & PIE Verification

The C++ for the General system is complete and **builds clean** (LyraEditor Win64 Development). The
StateTree *asset* and data assets must be authored in the editor (MCP cannot create StateTree assets).
This guide walks the authoring + verification. Do the build **with the editor closed** first — new
USTRUCTs/UPROPERTYs are not Live-Coding-safe.

Suggested content folder: `Plugins/GameFeatures/AshDraftCore/Content/AI/General/`.

---

## 1. Create the General StateTree — `ST_AshGeneral`

1. Content Browser → right-click → **Artificial Intelligence → State Tree**.
2. In the create dialog, **Schema = `StateTree AI Component`** (`UStateTreeAIComponentSchema`). Name it
   `ST_AshGeneral`.
3. Open it. Set the schema's **AIController Class = `AshGeneralController`** (Details panel, schema
   section). Leave **Context Actor Class = `AshGeneralCharacter`** (or Pawn) so bindings resolve.

### States (siblings under the Root, top to bottom = highest priority first)

The C++ tasks self-yield when a higher-priority premise appears, so all you need is the ordering plus
one enter condition per gated state, and a "completed → re-select" transition on each.

| Order | State name        | Enter Condition (Ash General: Has Threat) | Task (Ash General:)        |
|-------|-------------------|-------------------------------------------|----------------------------|
| 1     | `ReactToEnemy`    | `ThreatType = Enemy In Range`             | `Attack Target`            |
| 2     | `AssaultStronghold`| `ThreatType = Stronghold In Path`        | `Assault Stronghold`       |
| 3     | `ExecuteOrder`    | *(none)*                                   | `Execute Order`            |

For **each** state:
- Add the **Task** from the "Ash|General" category (the four nodes appear there).
- For states 1 and 2 add the **Enter Condition** "Ash General: Has Threat" and set its `ThreatType`.
- The `AIController` field on every node is a **Context** binding — it auto-binds to the schema's
  AIController; if it shows unbound, click it and pick the AIController context.
- Add a **Transition**: Trigger = **On State Completed** (covers Succeeded + Failed) → **Go to Tree
  Root** (re-selection). This is what lets a finished/failed sub-objective fall back, and a preempting
  threat take over.

Compile/save the StateTree (it should compile with no errors).

> Why this works: on every re-selection the Root picks the first sibling whose enter conditions pass.
> `ExecuteOrder` runs until it `Fails` itself (it returns Failed the moment a threat is sensed);
> `ReactToEnemy`/`AssaultStronghold` `Succeed` when their threat is gone. So the general always drifts
> back to the commander order between interruptions.

---

## 2. Create the General config — `DA_General_Enemy`

1. Content Browser → **Miscellaneous → Data Asset** → class **`AshGeneralConfig`**. Name `DA_General_Enemy`.
2. Fill in:
   - **Soldier Config** = an existing `DA_MassSoldierConfig` (the troops' unit type).
   - **Troop Count** = e.g. 60 (the data-driven army size).
   - **Troop Spawn Radius** / **Formation Radius** = e.g. 800 / 700.
   - **Enemy Engage Radius** / **Stronghold Detour Radius** = e.g. 1500 / 2500.
   - **Attack Range** = 175 (matches the basic-attack sweep reach).
   - **State Tree** = `ST_AshGeneral`.
   - **LOD** distances + **Think Intervals** — defaults are fine to start.

(Optional) duplicate as `DA_General_Ally` for an allied general.

---

## 3. Re-parent the enemy general Blueprint

The existing enemy general BP (Phase 5) becomes a StateTree general:

1. Open the enemy general Blueprint (the one parented to `AshEnemyGeneralCharacter`).
2. **File → Reparent Blueprint → `AshGeneralCharacter`**.
3. In Class Defaults:
   - **AI Controller Class = `AshGeneralController`** (should already default to it).
   - **Config = `DA_General_Enemy`**.
   - **Team Id = Enemy**.
   - **Default Abilities** = the same `GA_*` basic-attack ability the Phase 5 general used (so
     `TriggerBasicAttack` works).
   - Mesh / AnimBP / capsule: keep what the Phase 5 general had.
4. Compile & save. (If a separate ally general is wanted, make a BP child of `AshGeneralCharacter` with
   `DA_General_Ally`, Team = Ally.)

> Note: `AAshEnemyGeneralCharacter` and its BT controller still exist in code; re-parenting moves the
> placed instances onto the new StateTree general. The old class can be removed in a later cleanup once
> nothing references it.

---

## 4. Place & play

1. Place one (or more) general BP instances in the battle map, spaced from the player start.
2. Ensure the map has the usual `AAshBaseActor`s (the commander needs bases to assign objectives) and a
   NavMesh (`NavMeshBoundsVolume`) covering the play area (the general moves via NavMesh `MoveTo`).
3. PIE (or **Simulate** to watch without possessing the hero).

### Verify

- **Troops & formation:** the general spawns ~TroopCount soldiers; when it holds position they settle in
  a ring (~FormationRadius) around it rather than stacking on its point.
- **Follow + order:** the commander gives the general a base objective; it marches there and the troops
  move with it as a body.
- **Enemy preemption:** put a hostile (the hero, or an opposing general) within `EnemyEngageRadius` of
  the marching general → it breaks off to `ReactToEnemy`, faces and attacks; troops converge. When the
  enemy dies/leaves, it resumes the march.
- **Stronghold preemption:** with an enemy base inside `StrongholdDetourRadius` of its path, it detours
  to `AssaultStronghold`, then resumes after the base flips.
- **AI-LOD:** move a general far from the player; its think-rate drops (transitions/attacks visibly
  slow). Near the player it is responsive. (Add an on-screen print of the LOD level if you want a
  readout — `UAshGeneralSubsystem` holds it per general.)
- **Death parity:** the re-parented enemy general still fights the hero and dies correctly
  (`OnGeneralDied`), now StateTree-driven.

### If something is off
- Troops don't follow → confirm the general's `Config.SoldierConfig` is set and a NavMesh exists.
- General never moves → NavMesh missing, or no bases for the commander to target.
- Sub-objectives never trigger → check the `Has Threat` conditions' `ThreatType` and the engage/detour
  radii; confirm the enter-condition `AIController` bindings resolved.
- StateTree inert → confirm `DA_General_Enemy.StateTree` points at `ST_AshGeneral` and the BP is parented
  to `AshGeneralCharacter` with `AshGeneralController`.

After a clean PIE pass, fill the results into `Done/DONE_general_statetree_system.md` (Testing section)
and check Phase 22 in `Docs/PLAN.md`.
