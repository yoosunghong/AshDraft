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

- [ ] Phase 23: Large-Battle Combat Feel (general-vs-general field report)
  - Fixes four defects from a 30v30 general duel ("doesn't feel like a large-scale battle"). All
    tunables are data-driven (`UAshGeneralConfig`, `UAshMassSoldierConfig`, `UAshSoldierBehaviorConfig`).
  - **TroopCount = squads, not soldiers.** `UAshGeneralConfig::TroopCount` is now the number of
    5-soldier squads; the general spawns `TroopCount * 5` (default 6 → 30). See `AshGeneralCharacter::SpawnTroops`.
  - **Distributed deployment.** `AshMassSoldierSpawn::SpawnSoldiers` no longer scatters every soldier
    across one disc — each fireteam gets its own cluster centre (golden-angle spread within
    `TroopSpawnRadius`) and its members sit at the V slots, so squads start spread out, not intermixed.
  - **No more convergence on the generals.** `UAshMassFireteamProcessor` matchmaking reworked: hostile
    fireteams pair off **1:1 by mutual proximity** (greedy), the outnumbered remainder doubles up, and
    only fireteams with no enemy squad left in range fall back to a general — generals stop being a
    crowd magnet, the clash spreads into a brawl.
  - **Anti-climbing.** Same-team `SeparationRadius` 90→120 and combat anchor resistance 0.85→0.6;
    plus the kiting standoff keeps opposing bodies apart, so soldiers stop stacking on one point.
  - **Dynasty-Warriors kiting.** New per-soldier press/retreat oscillation (behavior processor owns the
    randomized phase; movement processor steers to a close strike standoff vs. a wider give-ground
    standoff) and **randomized attack cadence** (`AttackCooldownVariance`) so lines edge in and out
    trading blows on staggered timing instead of grinding in place.
  - Build verified (LyraEditor Win64 Development — Succeeded, 2026-06-28, editor closed). PENDING USER:
    restart the editor; set `DA_General_*` TroopCount to a squad count (e.g. 5–6); optionally tune
    `DA_*_Behavior` kiting fields + `AttackCooldownVariance`; PIE-verify squads deploy spread out, brawl
    squad-on-squad (not all on the generals), trade blows with ebb/flow, and no longer climb each other.
    For the kiting backpedal animation, author the locomotion blend space per
    `Docs/Guides/Phase23_Locomotion_BlendSpace_Setup_Guide.md`. See `Done/DONE_large_battle_combat_feel.md`.

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

- [ ] Phase 24: Global Combat + Engagement Deployment Director (Dynasty-Warriors battle)
  - Fixes "doesn't feel like a real battlefield" — two generals met and their armies formed one long
    grinding line. The old fireteam matchmaking was per-frame, local and proximity-greedy: every squad
    just attacked whatever was straight ahead. There was no *global plan*, no deliberate march to a
    designated foe, no circular spread.
  - **Global Combat state.** New `UAshBattleSubsystem` (the "Engagement Director"): a timer-driven world
    subsystem (no Tick — CLAUDE.md 18.3) that detects when two hostile **Generals** come within
    `BattleEncounterRadius` and forms a **Battle** (connected-component clustering supports N generals).
    `IsInCombat()` / `GetActiveBattleCount()` expose the state.
  - **Engagement deployment algorithm** (recomputed only on the 0.5 s replan, so assignments are
    *persistent* and a squad *commits*): gather both sides' fireteams; **balanced global matching**
    (greedy 1:1 by mutual proximity; the surplus side doubles up onto the nearest under-cap enemy →
    1v1 / 1v2, never 1v5, capped by `MaxAttackersPerEnemyFireteam`); **ring deployment** — each duel
    gets an evenly-spaced angular slot on a circle (`DuelRingRadius`) around the battle centre, ordered
    by current angle to minimise cross-running, so the melee becomes a *ring of simultaneous squad
    duels* instead of a line. `FAshEngagementAssignment` (enemy fireteam id + shared ring slot).
  - **March to a designated enemy, ignore everything en route.** `UAshMassFireteamProcessor` consumes
    the plan: a fireteam first **Deploys** straight to its ring slot (new `EAshFireteamState::Deploying`
    makes `UAshMassSoldierBehaviorProcessor` suppress local sensing, so the squad ignores every enemy
    it passes), then hands off to **Engaged** at the slot and duels its designated enemy. Out of combat
    the existing Phase 23 greedy proximity is the fallback — no regression to normal marching.
  - All tunables data-driven on `UAshGeneralConfig` (`BattleEncounterRadius`, `DuelRingRadius`,
    `MaxAttackersPerEnemyFireteam`, read as max() across the clashing generals) + the fireteam
    processor (`BattleSlotArrivalDistance`, `BattleEngageProximity`). `Ash.Battle.Debug 1` draws the
    battle centre, duel ring and slots. See `Done/DONE_global_combat_engagement_director.md`.
  - PENDING USER: rebuild with the **editor closed** (new subsystem/types + `UPROPERTY`s on
    `UAshGeneralConfig`; the new `EAshFireteamState::Deploying` value is enum-only so no struct layout
    change). Place an Ally and an Enemy `AAshGeneralCharacter` with troops; set `DA_General_*` battle
    fields; PIE-verify: when the generals close, both armies split into a ring of 1v1/1v2 squad duels
    around them (not a line), squads march past intervening enemies to reach their assigned foe, and the
    fight spreads out. Tune `DuelRingRadius` for the circle size; `Ash.Battle.Debug 1` to see the plan.
  - **Backward movement removed (follow-up).** The Phase 23 press/retreat *kiting* (the backpedal where
    soldiers gave ground to a wider standoff) is gone: a soldier now closes to its striking standoff and
    **holds**, never steering backward (same-team separation seats the spacing). Removed the kiting phase
    state (`FAshSoldierStateFragment::bAdvancePhase`/`CombatPhaseEndTime` — struct layout change, so the
    rebuild must be editor-closed) and the retreat-only `UAshSoldierBehaviorConfig` fields
    (`bEnableCombatKiting`, `RetreatStandoffScale`, `RetreatSpeedScale`, `Press/RetreatDuration*`,
    `KiteHysteresisBand`); kept `AttackStandoffScale` + `MinCombatSpacing` for the hold distance. The
    randomized attack cadence (`AttackCooldownVariance`) is unaffected. The backward locomotion
    blendspace/anim from Phase 23 is now optional (soldiers no longer drive a backpedal clip).

- [ ] Phase 25: Player-in-the-crowd combat fixes (three field defects)
  - Fixes three defects reported when the hero fights inside the soldier mass. All code-only; no new data.
    1. **Player trapped when surrounded** — the soldier proxy hit capsule is meant to be query-only/overlap
       (never blocking; crowd spacing is the steering separation), but nothing re-asserted that at runtime,
       so a blocking preset on `B_Soldier_Proxy` could wall the player in (can't move/rotate).
       `AAshSoldierProxyActor::AssignToEntity` now forces overlap-only collision (object type Pawn, ignore
       all, overlap Pawn) every assignment, so soldiers can never physically block movement.
    2. **Soldiers climb each other / rise when packed** — `UAshMassGroundProcessor` snapped Z to the first
       blocking ground-trace hit, and a Character capsule (hero/general) blocks `ECC_WorldStatic`, so a
       soldier overlapping a body climbed onto it. The trace is now a multi-trace that skips any pawn
       (hero/generals) and any Pawn-typed component (proxy capsules) and conforms only to real terrain.
    3. **Enemy soldiers ignore the player** — soldiers acquired targets only from the fireteam actor-target
       path (generals only) and the behavior sense grid (Mass entities only); the hero is neither, so no
       soldier could ever target it and they milled about the squad objective. The fireteam processor's
       actor-target fallback now also considers the hostile player pawn, the combat processor damages the
       hero (new `AAshHeroCharacter::ReceiveSoldierDamage`, mirroring the general) and the existing
       movement/behavior actor-target paths drive the approach + facing.
  - PENDING USER: rebuild with the **editor closed** (new `UFUNCTION` on `AAshHeroCharacter` — header
    change, not Live-Coding-safe); PIE-verify: the hero can walk/turn freely when fully surrounded, soldiers
    no longer stack vertically when packed, and enemy soldiers close in and attack the hero. See
    `Done/DONE_player_crowd_combat_fixes.md`.

- [ ] Phase 26: Battlefield Combat Feel — Stable Deployment + Melee Dissolve
  - **IMPLEMENTED (code); build verified; PENDING USER editor/PIE tuning.** Full spec: `Docs/Plan/Phase26.md`;
    `Done/DONE_battlefield_combat_feel.md`.
  - Review-driven phase fixing two field reports + structural debt in the Phase 20→24 combat stack:
    (1) squads **move back and forth** instead of committing to the matched enemy squad; (2) soldiers
    fight **too clustered with an unnaturally regular front line** (only ever attack the foe directly
    ahead — no flanking, no rear engagement, no chaos of contact). Chosen direction: **Hybrid** — keep
    the deliberate spread deployment (stabilised) and dissolve to individual melee on contact.
  - **Issue #1 root causes** (back-and-forth): (A) the duel ring is recomputed from scratch every 0.5 s
    replan, so committed fireteams get a *different* ring slot each time and re-march — `BaseAngle` and
    `2π/NumDuels` come from live midpoints (`AshBattleSubsystem::Replan`); (B) the engaged anchor
    `Target + normalize(MyAvg − Target)*420` **flips side** as a squad crosses the contact point; (C) on
    per-soldier target loss, movement snaps back to the 420-back formation anchor → the squad "breathes";
    (D) out-of-battle matchmaking is rebuilt every frame, greedy, **no hysteresis**.
  - **Issue #2 root causes** (clustered/regular): (E) engagement is at *squad* granularity (every member
    targets the matched slot soldier); (F) the rigid V is held into contact; (G) interpenetration is
    forbidden (stop-at-range + same-team-only separation + attacker anchor); (H) a soldier only ever sees
    the single nearest hostile, and the leash herds strays back — no path to a rear-rank opponent.
  - **Planned fixes** (all data-driven): stable **persistent duel ring** (cached axis + persistent slot
    index per fireteam + smoothed centre); non-flipping own-side anchor from the general→centre axis;
    target-loss **hold**; matchmaking **hysteresis**; a new shared **`AshEngagement`** helper module
    (`BalancedPairing` + `AcquireCappedNearest`) that removes the matching duplication between the battle
    subsystem and the fireteam processor (the "common algorithm helper" the review called for); a
    **`FAshMeleeSolver`** dissolve so engaged soldiers pair off with **distinct** opponents and surplus
    soldiers seek the nearest **open** enemy (→ organic flanking + **rear engagement**); opt-in weak
    opposing anti-stack so lines **jumble** without vertical climbing (Phase 25 ground guard preserved).
    Plus structural cleanup: **split** the `UAshMassFireteamProcessor::Execute` god-method and de-hardcode
    the V offsets / column count / thresholds into config.
  - New: `AshEngagement` helper module (`BalancedPairing` + `AcquireCappedNearest`) — the shared matcher
    that removes the duplication between `UAshBattleSubsystem` and `UAshMassFireteamProcessor`. The battle
    director now keeps **persistent ring state** (stable axis + per-fireteam slot index + smoothed centre,
    adaptive replan timer) so committed squads keep their seat instead of re-marching. The fireteam
    processor is split (decompose / stamp / fallback / apply) and **hands soldier targeting to the melee
    dissolve on contact** (no per-slot targets); the behavior processor's `AcquireCappedNearest` spreads
    attackers across distinct enemies (claim cap) so the surplus reaches **rear** ranks; the movement
    processor adds an **opt-in weak enemy anti-stack** (`EnemySeparationRadius`, default 0) so lines
    **jumble**. De-hardcoded the V offsets, column count, and stop/standby thresholds into data.
  - Build verified (LyraEditor Win64 Development — Succeeded, 2026-06-29, editor closed). PENDING USER
    (editor + PIE): restart the editor; optionally tune `DA_*_Behavior` melee fields + `DA_General_*` ring
    fields; PIE-verify squads deploy spread + hold (no back-and-forth), lines jumble on contact, surplus
    soldiers engage rear enemies, no vertical climbing. `Ash.Battle.Debug 1` to see the ring.

- [ ] Phase 27: Unit death animations + squad melee spacing (two field requests)
  - **Death animations (soldiers + generals), corpse removed after 5 s.** Soldiers no longer vanish the
    instant they die: `UAshMassDeathProcessor` now flips a new `FAshDeathFragment` (`bIsDying`/`DeathTime`)
    the frame health hits zero and only reaps the entity after `DeathDisplayDuration` (data-driven, default
    5 s). The representation processor keeps the soldier's **existing** proxy through the corpse window and
    plays the unit's new `UAshSoldierVisualConfig::DeathAnim` once (it never acquires a *new* proxy just for a
    corpse, so living soldiers keep priority on the bounded pool). The death anim plays in **single-node mode**
    (`PlayAnimation`, non-looping) so it **holds the final frame** for the whole window — no AnimBP authoring,
    no "returns to standing" blend-back (the original montage approach blended out to idle). The proxy restores
    `AnimationBlueprint` mode when recycled. A dying (zero-health) entity is inert — every gameplay processor
    already gates on `CurrentHealth > 0`, so it neither moves, fights, nor is targeted. Generals
    (`AAshGeneralCharacter`) and the hero (`AAshHeroCharacter`) play a `DeathAnim` (single-node) in
    `HandleDeath`; the general despawns after `DeathLifeSpan` (5 s), the hero persists (defeat flow).
  - **Squad-vs-squad melee spacing + general engagement (1v1 / 1v2).** Three combat-feel fixes:
    1. *Spacing:* an engaged fireteam fans its soldiers **abreast along the contact front on its own side**
       (`EngageLineSlot`, keyed on `SlotIndex`/`FireteamSize`; `EngageLineSpacing` + `EngageOwnSideOffset`)
       instead of collapsing every soldier onto the single contact point (the 10-on-one-spot pile).
    2. *Stable spread / no jitter:* the engage-line facing is low-pass filtered (`EngageFacingSmoothTime`,
       `SmoothedEngageFacing`, pass 2c) and never updated from a degenerate (intermixed) delta, so the opposing
       lines stay separated even in a merged melee and soldiers stop chasing a jittering anchor (was the source
       of the erratic motion).
    3. *Soldiers around the generals:* `UAshGeneralConfig::GuardFireteamCount` (default 1) keeps each general's
       nearest fireteam(s) as a personal guard — excluded from the duel ring in `UAshBattleSubsystem`, they
       follow the general's combat objective and fight around it, so the generals no longer duel alone in an
       empty ring clearing.
  - All tunables data-driven (`UAshMassDeathProcessor::DeathDisplayDuration`, `UAshSoldierVisualConfig::DeathAnim`,
    `AAshGeneralCharacter`/`AAshHeroCharacter::DeathAnim`, `UAshMassFireteamProcessor::EngageLineSpacing`/
    `EngageOwnSideOffset`/`EngageFacingSmoothTime`, `UAshGeneralConfig::GuardFireteamCount`). New
    `FAshDeathFragment` (added to the soldier archetype). See `Done/DONE_unit_death_animations.md` +
    `Done/DONE_squad_melee_spacing.md`.
  - Build verified (LyraEditor Win64 Development — Succeeded, 2026-06-29, editor closed). PENDING USER
    (editor + PIE): rebuild with the editor closed (new fragment/UPROPERTYs — not Live-Coding-safe); assign a
    death **AnimSequence** (`DeathAnim`) on each `DA_*_Visual` and on the general/hero Blueprints (no AnimBP
    work needed); PIE-verify dead bodies play the death anim, **hold the downed pose**, and despawn after 5 s;
    a 1v1/1v2 squad clash spreads into spaced ranks (no single-point pile); and the generals fight with their
    guard squads around them. Tune `GuardFireteamCount` for more troops near the generals, `EngageLineSpacing`/
    `EngageOwnSideOffset` for rank spread, `DuelRingRadius` (`DA_General_*`) for the ring size.

- [ ] Phase 28: Target Attack-Slot / Surround system (Musou crowd feel — field advice)
  - Review-driven phase applying external Dynasty-Warriors combat advice ("the key secret is target
    slotting + only a few attack while the rest circle and menace"). The prior stack already had the
    fireteam duel ring + count-capped melee dissolve; the genuine gaps were at the *individual soldier ↔
    individual target* granularity. Both are now closed by a per-target **attack-slot ring**.
  - **Inner / outer ring per target.** When N soldiers commit to one target (a Mass enemy **or** an actor
    general/hero) the first `ActiveAttackerCount` take **inner** slots — they close to the striking
    standoff and attack (the existing approach). The rest take the **outer** ring: a new
    `EAshSoldierState::Surround` where they hold a wider radius, **slowly orbit** (`SurroundOrbitSpeed`),
    face the target and menace, and **never strike** (the combat processor skips Surround). A circler is
    promoted to an inner slot the moment one frees (an inner attacker dies / retargets), so the crowd
    rotates. This is the cinematic cap from the brief — "only 3–5 attack while the rest circle".
  - **Slot angle = own approach bearing.** Each soldier rings the target on *its own side* (no
    cross-running); the outer bearing drifts each frame for the orbit; same-team separation fans the ring
    out. Inner/outer seats are stable frame-to-frame (kept while the target is kept) so soldiers don't
    flip-flop between striking and circling.
  - **Ratio-adaptive for free.** In a balanced army clash with a modest `MaxAttackersPerEnemySoldier`,
    squads still pair off 1v1/1v2 (no surround → no regression). Where soldiers outnumber targets —
    around the hero, a lone general, or the last survivor — they form a real surround ring (the iconic
    Musou shot; also makes the hero wade through crowds instead of being melted by a swarm).
  - All tunables data-driven on `UAshSoldierBehaviorConfig` (`ActiveAttackerCount`, `SurroundRingGap`,
    `SurroundOrbitSpeed`; `MaxAttackersPerEnemySoldier` reframed as the full ring capacity = strikers +
    waiters). Optional anim hook: `UAshSoldierAnimInstance::bInCombatStance` is pushed to the proxy so an
    AnimBP can show a guard/menace idle for circling soldiers — the surround works without it.
  - New: `EAshSoldierState::Surround` + `FAshSoldierStateFragment::SlotAngle` (fragment layout change → the
    rebuild must be **editor-closed**). Modified: behavior processor (inner/outer ring decision for Mass +
    actor targets), movement processor (outer-ring orbit destination + facing/hold for Surround),
    combat processor (Surround never strikes), representation processor + proxy + anim instance (combat
    stance hook). See `Done/DONE_target_attack_slots.md`.
  - PENDING USER: rebuild with the **editor closed** (new fragment field + enum value); to enable a visible
    surround, raise `DA_*_Behavior.MaxAttackersPerEnemySoldier` (e.g. 6–8) above `ActiveAttackerCount`
    (e.g. 3) and optionally tune `SurroundRingGap` / `SurroundOrbitSpeed`; PIE-verify: around the hero /
    a lone general a few soldiers strike while the rest circle and menace; a balanced army clash still
    pairs off without piling. Optional: branch the minion AnimBP idle on `bInCombatStance` for a guard pose.

- [ ] Phase 29: Mass Soldier Combo Attacks (morale-driven) + Idle / Death Animation fixes
  - **IMPLEMENTED (code); build verified; PENDING USER editor/AnimBP + PIE.** Two field requests.
  - **Combo attacks (1–3 hits per cycle, morale-driven).** A soldier's attack is now an *attack cycle* of
    1–3 quick hits instead of one swing. The chance scales linearly with the owning **general's morale**
    (level 1–5): up to **10%** for a 3-hit and **20%** for a 2-hit at morale 5 (e.g. morale 3 → 6% / 12%).
    The combat processor (`UAshMassCombatProcessor`) runs a per-soldier combo state machine: roll the
    length at cycle start, land hits paced by `ComboHitInterval` (data, 0.45 s), each hit deals
    `AttackPower` and plays its own montage; the cycle ends into the **3 s** default cooldown. New
    `FAshCombatFragment` fields (`TwoHitChance`/`ThreeHitChance`/`ComboHitInterval`/`ComboLength`/
    `ComboHitsLanded`/`TimeSinceComboHit`) + `FAshCombatEventFragment::AttackComboIndex`.
  - **Morale on the general.** `UAshGeneralConfig` gains `InitialMoraleLevel` (1–5, default 3),
    `MaxMoraleLevel`, `MaxTwoHitComboChance` (0.20), `MaxThreeHitComboChance` (0.10).
    `AAshGeneralCharacter` holds `MoraleLevel` with `GetMoraleLevel()`/`SetMoraleLevel()` (re-stamps the
    troops' chances) and stamps the chances onto each soldier at spawn. No gain/loss progression invented —
    `SetMoraleLevel()` is the seam for a future morale system. Plain spawner soldiers (no general) → 0
    chance (single hits).
  - **3 s cooldown + attack-slot return.** `AttackCooldown` defaults to **3.0** (`UAshMassSoldierConfig`,
    spawner + spawn-library fallbacks). After a cycle a soldier is "idle on cooldown"; the behavior
    processor (`UAshMassSoldierBehaviorProcessor`) treats it as a non-active striker (`bActiveStriker`), so
    it **yields its inner attack slot** to a waiting `Surround` soldier (it keeps its target and rings/
    menaces until the cooldown elapses) — the attack ring rotates.
  - **Combo montages (soldier_visual).** `UAshSoldierVisualConfig` gains `AttackComboMontages` (array, one
    montage per hit; falls back to `AttackMontage`). The proxy plays `AttackComboMontages[AttackComboIndex]`
    per landed hit (e.g. `Attack_C` = hit 1, `Attack_C_SetB` = hit 2 — the two Paragon minion attack
    variants; *not* a built-in 3-chain).
  - **Death → montage (consistency) + AnimBP idle/dead request.** `UAshSoldierVisualConfig::DeathAnim`
    (single-node `AnimSequence`) is replaced by `DeathMontage` (montage, like attack/hit). The proxy plays
    it via `Montage_Play` and sets `UAshSoldierAnimInstance::bIsDead`; the held downed pose is now an AnimBP
    **Dead state** driven by `bIsDead` (a montage blends back out on its own). The "idle not playing" defect
    is an **AnimBP authoring** gap, not code (the minion AnimBP needs an idle pose at `GroundSpeed≈0`).
  - Build verified (LyraEditor Win64 Development — Succeeded, 2026-06-29, editor closed). PENDING USER
    (editor + AnimBP + PIE): rebuild with the editor closed (new fragment `UPROPERTY`s — not Live-Coding-
    safe); in the **minion AnimBP** ensure an idle pose at zero speed and add a **Dead state** entered on
    `bIsDead` that holds the downed pose; author the per-unit `AttackComboMontages` + `DeathMontage` on each
    `DA_*_Visual`; set `InitialMoraleLevel` on each `DA_General_*` and `AttackCooldown = 3.0` on existing
    `DA_*` soldier configs; PIE-verify idle plays, soldiers occasionally land 2-/3-hit combos (raise morale
    to 5 for ~20%/10%), the ~3 s cooldown frees the slot for another soldier, and corpses play the death
    montage and hold the downed pose. See `Done/DONE_mass_soldier_combo_morale.md`.

- [ ] Phase 30: UI (Widget Blueprints) — Player HUD + Unit Health Bars
  - **IMPLEMENTED (code); PENDING USER build (editor closed) + WBP authoring + PIE.** All widgets are
    data/observation only (ARCHITECTURE.md 16): the UI reads gameplay state through delegates/subsystems
    and never mutates it.
  - **Base + concrete widget classes (UMG `UUserWidget` subclasses).** `UAshUserWidget` (abstract project
    base) → `UAshHUDWidget` (player HUD: health bar + mana bar, kill count, recently-struck panel),
    `UAshUnitHealthBarWidget` (over-head bar, cyan ally / red enemy), `UAshTargetInfoWidget` (struck-enemy
    name/affiliation/health). Elements bind by name (`BindWidgetOptional`) so the WBP layout is free.
  - **Smooth bars.** The HUD interpolates each bar's percent toward the live attribute fraction every
    frame (widget NativeTick, not Actor Tick), so health/mana glide down on damage; an optional trailing
    "chip" bar (`HealthBarTrail`/`ManaBarTrail`) lags further behind to show the amount just lost. Mana =
    the **Stamina** attribute (the project's secondary pool; no separate Mana attribute exists).
  - **Data source.** New `UAshCombatFeedSubsystem` (world subsystem) tracks the player's kill count and
    last-struck unit; the two damage choke points push strikes in — `AAshSoldierProxyActor::ReceiveMeleeHit`
    (Mass soldiers) and `UAshAttributeSet::PostGameplayEffectExecute` (generals). It records only the local
    player's strikes (AI-vs-AI is ignored). New `IAshTeamAgentInterface::GetAshDisplayName()` (+ `DisplayName`
    on hero/general/`UAshSoldierVisualConfig`) names the struck unit.
  - **Over-head bars ride the existing LOD.** `B_Soldier_Proxy` gains a screen-space `UWidgetComponent`
    (`HealthBarWidget`) driven from `SyncFromEntity`; a proxy only exists while promoted near the player, so
    the bar shows only "with the mesh when it is rendered". `UAshUIStatics` gives team→colour/label helpers.
  - New: `UI/AshUITypes` (`FAshStruckUnitInfo` + `UAshUIStatics`), `UI/AshCombatFeedSubsystem`,
    `UI/AshUserWidget`, `UI/AshHUDWidget`, `UI/AshUnitHealthBarWidget`, `UI/AshTargetInfoWidget`. Modified:
    Build.cs (+`UMG`), team interface (+`GetAshDisplayName`), hero/general (+`DisplayName`), visual config
    (+`DisplayName`), proxy (+widget component + strike report), attribute set (+strike report).
  - Partial (2026-06-30 via VibeUE): `WBP_AshHUD` (`/AshDraftCore/Game/UI/HUD/`), `WBP_AshUnitHealthBar`
    (`/AshDraftCore/Game/UI/Unit/`), and `WBP_AshTargetInfo` (`/AshDraftCore/Game/UI/`) authored with
    correct parent classes and all named bind-widget children. `BP_SoldierProxy.HealthBarWidget.widget_class`
    assigned to `WBP_AshUnitHealthBar`.
  - PENDING USER: add `WBP_AshHUD` to the viewport (Create Widget in HeroCharacter/PlayerController BeginPlay
    → Add to Viewport); set `DisplayName` on `DA_*_Visual`, general BP, and hero BP; PIE-verify per the guide.
    See `Done/DONE_ui_widgets.md` + `Docs/Guides/Phase30_UI_Widget_Setup_Guide.md`.

- [x] Phase 31: Hero Archetype & Progression Data
  - **IMPLEMENTED (code); build with editor closed; then author DA_Hero_* assets.**
  - `UAshHeroConfig` (`UPrimaryDataAsset`) — one asset per hero archetype (name, portrait, base stats,
    optional `AICharacterClass` pointer). Base stats are the "AI-tier" floor shared by every instance.
  - `FAshHeroStatBonuses` (`USTRUCT`) — flat additive player-upgrade bonuses. Added to base stats before
    `SetNumericAttributeBase` so GE modifiers (buffs/debuffs) still layer on top via the normal GAS stack.
  - `AAshHeroCharacter` gains `HeroConfig` + `StatBonuses`. `InitializeAttributes()` reads config base +
    bonuses; falls back to legacy `Initial*` floats when no config (backward-compatible). New
    `ApplyStatBonuses()` re-seeds GAS base values mid-game (save-game load seam).
  - `AAshGeneralCharacter` gains optional `HeroConfig`. When set, uses archetype base stats only (no bonuses).
  - PENDING USER: build with the editor closed (new `USTRUCT`/`UPROPERTY`s); author `DA_Hero_*` assets;
    assign on hero BP + general BPs that represent the same archetype; PIE-verify stat differences when
    `StatBonuses > 0`. See `Done/DONE_hero_archetype_progression.md`.

- [x] Phase 32: Hit-Reaction Stun + Knockback, and Hero Attack Improvements
  - **IMPLEMENTED (code); build verified (LyraEditor Win64 Development — Succeeded, 2026-07-01, editor closed).**
  - **Universal hit reaction (pushed back + stunned on being hit).** Every combatant — hero, general,
    Mass soldier — is now briefly **stunned** and **knocked back ever so slightly** when struck; while
    stunned it can neither move nor attack. Stun is a **GAS state**: the hero/general carry an ASC, so
    being hit adds the `Ash.State.Stunned` loose tag for a data-tunable window (cancels the in-flight
    attack; the basic-attack ability lists `Ash.State.Stunned`/`Ash.State.Dead` in `ActivationBlockedTags`
    so it can't re-fire while stunned; movement input is gated on the tag for the hero, and the
    `AAshGeneralCharacter` StateTree tasks freeze movement/orders via `IsStunned()`). Mass soldiers have
    no ASC, so they carry the analogous **`FAshStunFragment`** (StunTimeRemaining + decaying knockback);
    the movement processor owns the countdown + slide and suppresses steering, the combat processor skips
    striking while stunned. Knockback is a brief launch (GAS) / decaying velocity (Mass) away from the
    attacker. All applied at the existing damage choke points: `UAshGA_BasicAttack::PerformHitSweep`
    (hero→ASC target via the new `IAshTeamAgentInterface::ApplyHitReaction`; hero→soldier via the proxy
    `ReceiveMeleeHit`), and `UAshMassCombatProcessor` (soldier→soldier, and soldier→hero/general via
    `ApplyHitReaction`). Tunables: hero/general `HitReactStunDuration`/`HitReactKnockbackSpeed`;
    `UAshSoldierBehaviorConfig::StunDuration`/`KnockbackSpeed` (0 disables the flinch for a unit type).
  - **Hero attack improvements.** (1) *Forward lunge* — the basic attack steps the hero forward slightly
    on each hit via `LaunchCharacter` (`AttackLungeSpeed`, larger `DashAttackLungeSpeed` for the dash).
    (2) *Stop moving to attack* — activating the attack calls `StopMovementImmediately`, and the player
    cannot drive movement while `Ash.State.Attacking` is held, so the hero plants and commits instead of
    sliding through the strike. (3) *Dash Attack* — pressing attack after moving continuously for
    `DashAttackMoveSeconds` (default 2 s) opens with a `DashAttackMontage` instead of the normal combo
    starter (`AAshHeroCharacter::ShouldUseDashAttack()` tracks the run; the ability picks the dash montage
    for the opening hit and can chain into `ComboMontages[1]`).
  - New: `FAshStunFragment` (added to the soldier archetype). Modified: gameplay-tag-blocked basic attack,
    `IAshTeamAgentInterface` (+`ApplyHitReaction`), hero/general (stun + lunge + dash-run tracking),
    movement/combat processors, soldier proxy, behavior config, StateTree general tasks.
  - **Follow-up (field fixes + stun refactor).**
    - *Dash attack fixed:* the dash check ran AFTER `StopMovementImmediately` had zeroed the velocity it
      reads, so it never triggered — reordered so `ShouldUseDashAttack()` is evaluated first.
    - *Combo 3 & 4 now damage:* each combo step is guaranteed one damage sweep — if a step's montage has no
      Ash Melee Hit notify (or it hasn't fired), the ability sweeps once as a fallback when leaving the step
      (`bStepSwept`), so every hit lands regardless of notify authoring.
    - *Forward lunge made visible:* default `AttackLungeSpeed` 250→500, `DashAttackLungeSpeed` 700→1000
      (the 250 step was ~16 cm under the 2000 braking decel). NOTE: an in-place root-motion attack montage
      overrides the lunge — disable its root motion or author it to move forward.
    - *Stun refactored to GE + game-wide rule data:* stun for the hero/general is now applied via a
      **`UAshGameplayEffect_Stun`** (`GE_State_Stunned`, HasDuration, grants `Ash.State.Stunned`, duration
      via `Ash.Data.StunDuration` SetByCaller) instead of a loose tag + manual timer — so weapon/skill data
      can vary the duration later by swapping the effect or the SetByCaller value. New game-wide
      **`UAshCombatRulesSettings`** (UDeveloperSettings, Project Settings → Game → "Ash Combat Rules") holds
      `NewStunSourceImmunity` (default 2 s): after being stunned, a **different** attacker cannot stun the
      victim again until the window elapses (the **same** attacker's continuing combo always may), so the
      player gets a guaranteed counterattack window and infinite stun-locking is impossible. Applied
      uniformly to hero, general, and Mass soldiers (shared `AshCombat::ApplySoldierStun` + the new
      `FAshStunFragment::LastStunSourceId/LastStunTime`).
  - PENDING USER: rebuild with the **editor closed** (new fragment/`UPROPERTY`s + new types — not
    Live-Coding-safe); on the hero `GA_BasicAttack` assign a `DashAttackMontage` (carry the Ash Melee Hit
    notify) and tune `AttackLungeSpeed`/`DashAttackLungeSpeed`; tune `Project Settings → Game → Ash Combat
    Rules → NewStunSourceImmunity` if 2 s is not the desired window; optionally tune `DA_*_Behavior`
    `StunDuration`/`KnockbackSpeed` and the hero/general `HitReact*` values; PIE-verify: struck units flinch
    back + can't act briefly, a single enemy's combo stun-locks but a second enemy can't pile on for 2 s,
    the hero lunges forward and stops to attack, a full 4-hit combo all deals damage, and a 2 s+ run into
    attack plays the dash attack. See `Done/DONE_hit_reaction_stun.md` + `Done/DONE_hero_attack_improvements.md`.

