# Phase 5: Enemy General Character and AI

Goal: Implement a high-importance enemy general using Character-based AI.

C++ foundation implemented this session (needs a closed-editor rebuild — new UCLASS
types). Asset creation + PIE verification follow the Phase 4 workflow.

- [x] Create `AAshEnemyGeneralCharacter`
  - `Character/AshEnemyGeneralCharacter.h/.cpp`: GAS character (ASC + AshAttributeSet,
    reuses the Phase 4 foundation), team = Enemy, AutoPossessAI, grants the shared
    `UAshGA_BasicAttack`.
- [x] Add AIController class
  - `AI/AshEnemyGeneralController.h/.cpp`: runs the assigned Behavior Tree on possess,
    exposes `TargetActor`/`TargetDistance` BB key names, `StopBehavior()` for death.
- [x] Add Blackboard asset
  - `/AshDraftCore/AI/BB/BB_EnemyGeneral` with keys `TargetActor` (Object/Actor) and
    `TargetDistance` (Float). See `Docs/Guides/Phase5_BehaviorTree_Setup.md`.
- [x] Add Behavior Tree asset
  - `/AshDraftCore/AI/BT/BT_EnemyGeneral` — Selector with `Ash Detect Player` service +
    a `TargetActor IsSet`-guarded Sequence [MoveTo(AcceptRadius 150) +
    `Ash Activate Ability (Input Tag)` + Wait]. Authored by the user; assigned to
    `B_EnemyGeneralController`.
  - Done via MCP: `B_EnemyGeneral` + `B_EnemyGeneralController` Blueprints created and
    wired (abilities + AIControllerClass); general placed in `BaseLevel`;
    NavMeshBoundsVolume + RecastNavMesh generated.
- [x] Implement player detection
  - `AI/AshBTService_DetectPlayer` (writes `TargetActor`/`TargetDistance`, sets controller
    focus, hysteresis); wired into `BT_EnemyGeneral`.
- [x] Implement approach behavior
  - Built-in BT MoveTo to `TargetActor` over the RecastNavMesh.
- [x] Implement basic attack behavior
  - `AI/AshBTTask_ActivateAbilityByInputTag` triggers the shared basic attack via the
    ASC input path; wired into `BT_EnemyGeneral`.
- [x] Implement hit and death handling
  - `AshEnemyGeneralCharacter`: Health-change delegate → `HandleDeath()` (stops BT,
    cancels abilities, disables collision/movement, broadcasts `OnGeneralDied`,
    `Ash.State.Dead`, death lifespan).
- [x] Add morale placeholder
  - `AshEnemyGeneralCharacter::CurrentMorale` (0-100, tunable; no behavior yet).
- [x] Verify that the enemy general can chase and attack the player
  - CONFIRMED in PIE (2026-06-21): with `BT_EnemyGeneral` assigned, the general detected
    the player, chased over the NavMesh, and executed the basic attack on the hero.
- [x] Create `Done/DONE_enemy_general_ai.md`