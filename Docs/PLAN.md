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
- [x] Phase 14: Flow Field Navigation PoC
- [ ] Phase 15: Battlefield PoC Map
  - EDITOR phase for the map itself (authoring a level, placing bases/spawns/lanes). The
    supporting code already exists (`AAshBaseActor` + `bIsMainBase`, `AAshMassSoldierSpawner`,
    flow-field/commander subsystems). See `Docs/Guides/Phase15-19_Editor_Work_And_Verification.md` §15.
  - Added (code): **Mass soldier skeletal meshes + animations** — `AAshSoldierProxyActor` is now a
    `USkeletalMeshComponent` body with attack/hit montages; new `FAshCombatEventFragment` carries
    one-shot attack/hit flags the combat processor sets and the representation processor plays on the
    proxy. See `Done/DONE_mass_soldier_skeletal_anim.md`.
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
- [x] Phase 19: PoC Final Review
  - VERIFICATION-ONLY phase (no codebase deliverable): exercise every prior system in PIE and write
    the review. A diagnostic per-system verification checklist is in the guide §19; fill in
    `Done/DONE_poc_final_review.md` after the pass.

