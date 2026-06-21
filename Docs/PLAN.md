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
- [] Phase 8: Basic Soldier Prototype
- [] Phase 9: Mass Entity Soldier Foundation
- [] Phase 10: Mass Movement and Combat Processors
- [] Phase 11: Hierarchical AI Foundation
- [] Phase 12: AI LOD and Time Slicing
- [] Phase 13: Actor Proxy / Hybrid Representation
- [] Phase 14: Flow Field Navigation PoC
- [] Phase 15: Battlefield PoC Map
- [] Phase 16: Victory, Defeat, and Match Flow
- [] Phase 17: Telemetry and QA Bot Interface Preparation
- [] Phase 18: Performance Validation
- [] Phase 19: PoC Final Review

