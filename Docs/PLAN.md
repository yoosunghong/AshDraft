# Checklist

## Phase detail: Docs/Plan/

- [x] Phase 1: Gameplay Tags, Input, and Basic Data Setup
- [x] Phase 2: Experience and Pawn Setup
- [x] Phase 3: Playable Hero Character
- [x] Phase 4: GAS Combat Foundation
- [x] Phase 5: Enemy General Character and AI
- [x] Phase 6: 8-Direction Combat Slot System
- [x] Phase 7: Base / Stronghold System
- [x] Phase 8: Basic Soldier Prototype
- [x] Phase 9: Mass Entity Soldier Foundation
- [x] Phase 10: Mass Movement and Combat Processors
- [x] Phase 11: Hierarchical AI Foundation
- [x] Phase 12: AI LOD and Time Slicing
- [x] Phase 13: Actor Proxy / Hybrid Representation
- [x] Phase 14: Flow Field Navigation
- [ ] Phase 15: Battlefield Map
  - EDITOR phase for the map itself (authoring a level, placing bases/spawns/lanes). The
    supporting code already exists (`AAshBaseActor` + `bIsMainBase`, `AAshMassSoldierSpawner`,
    flow-field/commander subsystems). See `Docs/Guides/Phase15-19_Editor_Work_And_Verification.md` §15.
  - Added (code): **Mass soldier skeletal meshes + animations (data-driven unit types)** —
    `AAshSoldierProxyActor` is a generic `USkeletalMeshComponent` template dressed at runtime from a
    `UAshSoldierVisualConfig` (mesh/AnimBP/montages); `UAshMassSoldierConfig` is now the unit-type
    asset (stats + `Visual`). New `FAshVisualFragment` links each entity to its visual set and
    `FAshCombatEventFragment` carries one-shot attack/hit flags the combat processor sets and the
    representation processor plays on the proxy. Adding a unit type is pure data authoring.
    See `Done/DONE_mass_soldier_skeletal_anim.md`.
  - Added (code): **editor-configurable victory/defeat** — `UAshMatchRulesConfig` + `AAshMatchRulesActor`
    let designers toggle win/lose conditions (all bases captured, enemy generals eliminated, hero death,
    main base lost) and a time limit per level; `UAshMatchSubsystem` consumes them (defaults preserve the
    Phase 16 behavior). See `Done/DONE_editor_match_rules.md`.
  - PENDING USER (editor + PIE): assign the proxy skeletal mesh/AnimBP/montages; create a `DA_MatchRules`
    and place an `AshMatchRulesActor`; build (LyraEditor Win64) then PIE-verify animations + each condition.
- [x] Phase 16: Victory, Defeat, and Match Flow
  - Complete (code): `UAshMatchSubsystem` (world state machine NotStarted→InProgress→Victory/Defeat;
    binds `UAshBaseSubsystem::OnAnyBaseOwnershipChanged` + hero `OnHeroDied`; victory = player side
    owns all bases, defeat = hero death or a player-side main base captured; `OnMatchStarted` /
    `OnMatchEnded` / `OnMatchStateChanged` delegates + on-screen result fallback). Added
    event-driven hero death (`AAshHeroCharacter::OnHeroDied` + `HandleDeath`, mirroring the general)
    and `AAshBaseActor::bIsMainBase`/`IsMainBase()`. `EAshMatchState`/`Result`/`EndReason` types.
  - Build verified (LyraEditor Win64 Development — Succeeded, 2026-06-22). PENDING USER (editor +
    PIE): proper result UMG widget bound to `OnMatchEnded`; flag the ally/enemy home bases
    `bIsMainBase=true` in the level; PIE-verify each end condition. See guide §16.
- [x] Phase 17: Telemetry and QA Bot Interface Preparation
  - Complete (code): `FAshObservation` / `FAshAction` + log structs (`AshQABotTypes.h`);
    `UAshTelemetrySubsystem` — `BuildObservation()` (player vitals/pose, nearby enemy count, base
    ownership, current-objective string, match state), `ApplyAction()` (move + camera + ability-tag
    presses = the Step() seam), combat/base/match-result logs (capped, auto-subscribed), and JSON
    export (`GetObservationJson`/`ExportToJson`/`ExportToFile`). Combat log fed from the GAS damage
    choke point (`UAshAttributeSet::PostGameplayEffectExecute`). Build.cs += `Json`,`JsonUtilities`.
  - Build verified (LyraEditor Win64 Development — Succeeded, 2026-06-22). PENDING USER (PIE):
    confirm JSON file under `Saved/AshTelemetry/`, sanity-check fields. Known limit: nearby-enemy
    count is actor-based (Mass entities not yet counted). See guide §17.
- [x] Phase 18: Performance Validation
  - Complete (code/harness): CVars `Ash.Mass.LOD` / `Ash.Mass.TimeSlice` wired into
    `UAshMassLODProcessor` (off = full-fidelity / per-frame recompute) via `AshPerf::` accessors;
    console commands `Ash.Perf.SpawnSoldiers <n>` (drives every spawner via new
    `SetDesiredSpawnCount`/`Respawn`) and `Ash.Perf.Report` (FPS + soldier/base/proxy counts +
    toggle state). The harness is the repeatable driver; the actual numbers are a PIE measurement.
  - Build verified (LyraEditor Win64 Development — Succeeded, 2026-06-22). PENDING USER
    (measurement): run 100/300/500/1000 with `stat unit`/`stat mass`, compare LOD and time-slicing
    on/off, fill in the results table + bottlenecks in `Done/DONE_performance_validation.md`. See guide §18.
- [x] Phase 19: Final Review
  - VERIFICATION-ONLY phase (no codebase deliverable): exercise every prior system in PIE and write
    the review. A diagnostic per-system verification checklist is in the guide §19; fill in
    `Done/DONE_final_review.md` after the pass.
- [ ] Phase 21: Player ↔ Mass Soldier Hit Interaction (query-only hit capsule)
  - Makes Mass soldiers damageable by the hero/general melee. The hero attack already sweeps
    `SweepMultiByChannel(ECC_Pawn, Sphere)` and applies a GE to actors with an ASC; soldiers have no
    ASC. Added a **query-only `UCapsuleComponent` on `AAshSoldierProxyActor`** (overlaps the Pawn
    channel only — never blocks movement, never simulates physics, never pushes other soldiers, so
    crowd spacing stays the steering separation) as the sweep's hit target. It is enabled only while
    the proxy represents a live entity and is sized per unit type from `UAshSoldierVisualConfig`
    (`CapsuleRadius`/`CapsuleHalfHeight`); it is visible/tunable on `B_Soldier_Proxy`.
  - `AAshSoldierProxyActor::ReceiveMeleeHit(Damage, Instigator)` routes the actor-space hit back into
    the entity's `FAshHealthFragment` via the Mass entity manager and raises the hit-react event
    (Mass stays authoritative; the death processor removes casualties). `UAshGA_BasicAttack::
    PerformHitSweep` branches on proxy hits: team-checked (no friendly fire) and damaged by the
    attacker's `AttackPower` attribute (the same source magnitude the GE uses — no magic number).
  - Decision: a query-only proxy capsule (bounded to promoted proxies) is the AAA-correct hit model;
    per-entity physics collision is NOT used (it is the wrong tool at army scale and was the cause of
    the Phase 20 crowd shaking). See `Done/DONE_player_soldier_hit_interaction.md`.
  - PENDING USER: rebuild (header changed — full build / editor restart, not just Live Coding);
    optionally tune the capsule size on `DA_*_Visual`; PIE-verify the hero can kill enemy soldiers but
    not allied ones.
- [ ] Phase 20: Mass Soldier Local AI (Behavior State Machine) + Combat / Collision Polish
  - Detail: `Docs/Plan/Phase20.md`. Gives every Mass soldier a data-driven **local behavior state
    machine** (`FollowSquad`/`Engage`/`Attack`) evaluated by `UAshMassSoldierBehaviorProcessor` —
    local-only sensing (no macroscopic search; soldiers follow the squad/general objective and only
    engage enemies within a small `LocalSenseRadius`, leashed to the objective). Decision: a
    data-oriented FSM over Mass fragments is the AAA-correct "state tree" for army-scale soldiers;
    Unreal StateTree stays reserved for the General/Squad layer (PROPOSAL.md §15.2/15.3/15.5).
  - Fixes the three field defects, all data-driven via new `UAshSoldierBehaviorConfig`:
    1. **Buried in ground** — soldiers had a frozen spawn-Z and no terrain follow; new
       `UAshMassGroundProcessor` conforms visible soldiers' Z to the ground (per-mesh art offset
       stays on `UAshSoldierVisualConfig`).
    2. **Attacking the wrong way** — proxy faced raw velocity (zero/sideways in melee); facing is now
       Mass-authoritative `FacingYaw`, target-aware and interpolated (face the enemy while striking).
    3. **Shaking / bigger army shoving** — separation reworked to **same-team only**, relaxed +
       clamped (no oscillation), with **combat anchoring** so attacking soldiers hold the line.
  - Combat processor reduced to damage application (no map-wide acquisition; the behavior processor
    owns target selection + AI time-slice).
  - PENDING USER (editor + PIE): author `DA_<Unit>_Behavior`, link from `DA_MassSoldierConfig.Behavior`,
    set per-mesh `MeshRelativeLocation.Z`; build then PIE-verify burial / facing / no-shake.
    See `Done/DONE_mass_soldier_local_ai.md` + `Docs/Guides/Phase20_MassSoldier_Behavior_Setup_Guide.md`.
- [ ] Phase 22: General StateTree System (operational AI layer)
  - Partial (code complete; editor authoring + PIE pending). Adds the missing operational layer between
    the commander and the soldiers: **AAshGeneralCharacter** — a high-fidelity, team-agnostic Character
    (not Mass) driven by an Unreal **StateTree** (via **AAshGeneralController** + `UStateTreeAIComponent`).
    Unifies/replaces the Phase 5 BT enemy general (same GAS foundation + event-driven death).
  - Control flow: Commander (strategic, which base) → General (StateTree, operational) → squad objective
    (= the general's live position, with a data-driven `FormationRadius`) → Mass soldiers (existing
    follow/engage/return). The general **spawns its own squad** of `UAshGeneralConfig::TroopCount`
    soldiers on BeginPlay via the new shared `AshMassSoldierSpawn::SpawnSoldiers` library (the Phase 9
    spawner was refactored onto the same path).
  - Sub-objective preemption (the brief): StateTree siblings ordered AttackTarget > AssaultStronghold >
    ExecuteOrder, each gated by a `HasThreat` condition; the lower-priority tasks self-yield when a
    higher-priority premise appears, so an enemy encountered en route or a stronghold in the path
    temporarily preempts the commander order and the general resumes it afterward. Threat sensing lives
    on the general, throttled by its think-rate.
  - **AI-LOD = actor-level** (answer to the design question): generals are Characters, so
    `UAshGeneralSubsystem` (a registry + the commander seam) throttles each general's StateTree
    think-rate by distance to the player from a single timer (no per-general Tick), reusing the LOD ring
    idea. `FAshSquadState.FormationRadius` makes the troops ring a stationary general.
  - New: `UAshGeneralConfig`, `UAshGeneralSubsystem`, `AAshGeneralCharacter`, `AAshGeneralController`,
    `FAshSTTask_ExecuteOrder/AttackTarget/AssaultStronghold` + `FAshSTCondition_HasThreat`,
    `AshMassSoldierSpawnLibrary`. Modified: commander (assign to generals, skip general-owned squads),
    squad subsystem/types (`FormationRadius`), movement processor (ring arrival), Build.cs (StateTree
    modules), gameplay tags.
  - PENDING USER (editor + PIE): build with the **editor closed** (new structs/UPROPERTYs — not
    Live-Coding-safe); author `ST_AshGeneral` (AI schema) + `DA_General_*`; re-parent the enemy general
    BP to `AAshGeneralCharacter` with `AAshGeneralController`; place generals; PIE-verify follow/
    formation, sub-objective preempt+resume, and LOD think-rate falloff. See
    `Done/DONE_general_statetree_system.md` + `Docs/Guides/Phase22_General_StateTree_Setup_Guide.md`.

- [ ] Phase 20.1: Engage-on-contact + distributed deployment (follow-up to two field reports)
  - **Combat on contact while marching** — the leash was measured from the squad's *final* objective,
    so an army marching toward a distant objective via flow field had every soldier beyond the leash
    and never engaged en route (combat only began once they neared the destination). Reworked: a
    soldier now engages any hostile that enters `LocalSenseRadius` immediately and the leash
    (`MaxLeashFromObjective`) is measured from its **engage anchor** (where it first made contact) —
    it fights what crosses its path, chases only that far, then returns to the flow field when the
    enemy dies or it strays past the leash. New `FAshSoldierStateFragment::EngageAnchor`/`bEngaged`.
  - **Distributed deployment / anti-shake** — soldiers steered at full speed to the *exact* objective
    point and a trapped centre was shoved every frame (ram-and-rebound shaking). Movement now (a)
    **eases speed in** within `ArrivalSlowdownRadius` of a group objective, and (b) **stops pressing
    in** once a same-team soldier already occupies the space ahead (nearer the goal), so rear ranks
    fan out into a stable, minimum-distance formation instead of piling on one point.
  - All tunables data-driven (`UAshSoldierBehaviorConfig`: leash now anchor-relative, new
    `ArrivalSlowdownRadius`); processor fallbacks preserved. See `Done/DONE_mass_soldier_engage_and_deploy.md`.
  - PENDING USER: rebuild with the **editor closed** (new `UPROPERTY`s on Mass fragments/struct change
    reflection — not Live-Coding-safe); PIE-verify mid-march engagement and no centre shake.

