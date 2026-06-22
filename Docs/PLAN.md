# Checklist

## Phase detail: Docs/Plan/

- [x] Phase 1: Gameplay Tags, Input, and Basic Data Setup
- [x] Phase 2: Experience and Pawn Setup
- [x] Phase 3: Playable Hero Character
- [x] Phase 4: GAS Combat Foundation
  - Complete: full GAS foundation in C++ (`UAshAbilitySystemComponent`,
    `UAshAttributeSet`, `UAshGameplayEffect_Damage` + `UAshDamageExecution`,
    `UAshGameplayAbility` base + `UAshGA_BasicAttack`, ASC/AttributeSet wired into
    `AAshHeroCharacter` with input-tag ability binding). Damage verified in PIE via
    `AAshGASTargetDummy` (100→0 at −25/hit, 2026-06-21).
- [x] Phase 5: Enemy General Character and AI
  - Complete: `AAshEnemyGeneralCharacter` + `AAshEnemyGeneralController` +
    `UAshBTService_DetectPlayer` + `UAshBTTask_ActivateAbilityByInputTag`; `BB_EnemyGeneral`,
    `BT_EnemyGeneral`, `B_EnemyGeneral`, NavMesh. Verified in PIE: detect → chase → attack
    (2026-06-21).
- [x] Phase 6: 8-Direction Combat Slot System
  - Complete: reusable `UAshCombatSlotComponent` (N-slot ring, `EAshCombatSlotState`
    lifecycle, `RequestSlot`/`ReleaseSlot`/`GetSlotWorldLocation`/`FindBestAvailableSlot`,
    owner-yaw-aware geometry), data-driven via `UAshCombatSlotConfig`, debug ring viz
    (Tick gated on `bDrawDebugSlots`). Verification harness `AAshCombatSlotTestActor`.
    Pending user C++ build + PIE run to confirm on-screen (2026-06-22).
- [x] Phase 7: Base / Stronghold System
  - Complete: `AAshBaseActor` (durability-capture model, sphere capture volume,
    presence-driven defender/attacker counting, contested freeze, timer-driven update,
    recovery delay) data-driven via `UAshBaseConfig`; ownership/durability/contested
    delegates + BP UI hooks; `UAshBaseSubsystem` registry + global ownership-change
    broadcast for AI; shared `IAshTeamAgentInterface` + `UAshTeamStatics` team
    resolution (implemented on hero/general/dummy). Pending user C++ build + PIE run
    to confirm a capture on-screen (2026-06-22).
- [x] Phase 8: Basic Soldier Prototype
  - Complete: `UAshSoldierConfig` data model + temporary `AAshSoldierCharacter` (team via
    `IAshTeamAgentInterface`, authoritative scalar health, NavMesh movement toward an
    objective, nearest-hostile acquisition + melee attack on cooldown, event-driven death
    that drains the nearest owned base via `NotifyDefenderDied`). Timer-driven think loop
    (no Tick, no per-soldier BT); `UAshSoldierSubsystem` registry for target/team queries.
    Pending user C++ build + PIE run per `Docs/Guides/Phase8_Soldier_Setup.md` (2026-06-22).
- [x] Phase 9: Mass Entity Soldier Foundation
  - Complete: Mass modules wired for UE5.8 (`MassCore`/`MassEntity` engine modules +
    `MassGameplay` plugin for `MassCommon`/`MassSpawner`); six `FMassFragment` data
    fragments (`FAshTeamFragment`, `FAshHealthFragment`, `FAshMovementFragment`,
    `FAshCombatFragment`, `FAshSquadFragment`, `FAshLODFragment`) mirroring the Phase 8
    soldier scalars; `UAshMassSoldierConfig` tunables; `AAshMassSoldierSpawner` builds
    the archetype and batch-creates/seeds/owns entities via `FMassEntityManager` (no
    processors, no Actor Tick). Defaults to 300 entities, logs count, draws a point per
    entity. Build verified (LyraEditor Win64); pending user PIE run per
    `Docs/Guides/Phase9_Mass_Soldier_Setup.md` (2026-06-22).
- [x] Phase 10: Mass Movement and Combat Processors
  - Complete (code): `UAshMassMovementProcessor` (steer to target/squad objective/nearest
    capturable base + integrate position), `UAshMassCombatProcessor` (uniform spatial-grid
    target acquisition + simple damage, LOD-throttled), `UAshMassDeathProcessor` (deferred
    DestroyEntity at 0 HP). All UE5.8-idiom auto-registered `UMassProcessor`s on the
    PrePhysics phase, game-thread, no Actor Tick.
  - VERIFIED 2026-06-22 (MCP PIE on BaseLevel): Mass simulation ticks; two spawners
    (Ally/Enemy, 400 each) battle and `UAshMassDeathProcessor` reaps continuously. The
    "are processors running" risk is cleared. See `Docs/Guides/Phase10-13_Mass_AI_Setup.md` §7.
- [x] Phase 11: Hierarchical AI Foundation
  - Complete (code): `UAshSquadSubsystem` (squad state registry + objectives + aggregates),
    `UAshCommanderSubsystem` (assigns attack/defend orders from base ownership; reacts to
    `UAshBaseSubsystem::OnAnyBaseOwnershipChanged`), `UAshMassSquadTrackingProcessor`
    (aggregates avg position + alive count back into squads). Spawner now registers its
    squad + notifies the commander.
  - PARTIAL (MCP PIE 2026-06-22): subsystems init, squads register, neutral mid-base placed;
    base ownership-change path confirmed (`LogAshBase: captured 0 -> 1`). Still needs a visual
    PIE pass to confirm squads converge on the correct base + re-target on flip. See guide §7.
- [x] Phase 12: AI LOD and Time Slicing
  - Complete (code): `UAshMassLODProcessor` — distance-to-player LOD (0..3), per-level
    update intervals consumed by the combat processor, time-sliced recompute
    (1/N batches per frame), per-LOD debug point draw. Camera-visibility + objective-
    importance LOD factors are stubbed (distance-only) — noted for later.
  - PENDING USER (visual PIE + perf): per-frame LOD debug points draw in the PIE game window
    only (not the editor-viewport capture), so LOD-colour/ring confirmation and the `stat unit`
    perf compare on/off need the user's eyes. See guide §7.
- [x] Phase 13: Actor Proxy / Hybrid Representation
  - Complete (code): `AAshSoldierProxyActor` (passive pooled view), `UAshSoldierProxyPool`
    (bounded acquire/release/recycle), `UAshMassRepresentationProcessor` (promote LOD0 →
    proxy, demote/transfer-back, sweep destroyed; max-proxy cap). Mass stays authoritative.
  - EDITOR WORK DONE 2026-06-22: `/AshDraftCore/Game/Mass/BP_SoldierProxy` created (mesh =
    engine Cube). Processor made `UCLASS(config=Game)` and `ProxyClass`/LOD tunables config-backed
    in `Config/DefaultGame.ini` so the wiring now persists (guide §8) — REQUIRES REBUILD to take
    effect. Capped-promotion still needs a visual PIE pass.
- [] Phase 14: Flow Field Navigation PoC
- [] Phase 15: Battlefield PoC Map
- [] Phase 16: Victory, Defeat, and Match Flow
- [] Phase 17: Telemetry and QA Bot Interface Preparation
- [] Phase 18: Performance Validation
- [] Phase 19: PoC Final Review

