# Phase 5 — Behavior Tree & Blackboard setup (manual editor step)

The MCP editor bridge can create Blueprints and place actors, but it **cannot author
Blackboard keys or Behavior Tree graphs** (those toolsets are read-only). These two
assets must be made by hand in the editor. Everything else is already done:

- `B_EnemyGeneral` (parent `AshEnemyGeneralCharacter`) — granted `GA_Hero_BasicAttack`,
  AIControllerClass = `B_EnemyGeneralController`. Placed in `BaseLevel` at (-310, 900, -332).
- `B_EnemyGeneralController` (parent `AshEnemyGeneralController`) — created, BT slot empty.
- `NavMeshBoundsVolume` + `RecastNavMesh` covering the play area (MoveTo can path).

The custom C++ BT nodes are compiled and appear in the BT palette:
**Ash Detect Player** (service) and **Ash Activate Ability (Input Tag)** (task).

## 1. Blackboard `BB_EnemyGeneral`

Content Browser → `/AshDraftCore/AI` → **Add → Artificial Intelligence → Blackboard**.
Name it `BB_EnemyGeneral`. Add two keys:

| Key Name         | Type   | Notes                         |
| ---------------- | ------ | ----------------------------- |
| `TargetActor`    | Object | Base Class = `Actor`          |
| `TargetDistance` | Float  |                               |

Key names must match exactly (the controller/service reference them by name).

## 2. Behavior Tree `BT_EnemyGeneral`

Same folder → **Add → Artificial Intelligence → Behavior Tree**. Name it
`BT_EnemyGeneral`. Open it and set **Blackboard Asset = `BB_EnemyGeneral`** (details panel).

Build this graph:

```
ROOT
└── Selector
    ├── [Service] Ash Detect Player
    │     • Blackboard Key (target)      = TargetActor
    │     • Target Distance Key          = TargetDistance
    │     • Sight Radius / Lose Sight     = 2000 / 2500 (defaults are fine)
    └── Sequence  "Engage"
        ├── [Decorator] Blackboard
        │     • Key Query        = Is Set
        │     • Blackboard Key   = TargetActor
        │     • Observer aborts  = Self
        ├── Move To
        │     • Blackboard Key      = TargetActor
        │     • Acceptable Radius   = 150
        ├── Ash Activate Ability (Input Tag)
        │     • Input Tag = Ash.InputTag.Attack.Basic   (already the default)
        └── Wait
              • Wait Time = 1.2   (attack pacing)
```

- The **Ash Detect Player** service goes *on the Selector* (right-click the Selector →
  Add Service) so it ticks every frame the tree runs, keeping `TargetActor` current.
- The **Blackboard decorator** on the Sequence makes the general only chase/attack when
  a target is set; with no target the Selector falls through (idle).

Save both assets.

## 3. Tell me when done

Once `BT_EnemyGeneral` exists I'll finish over the MCP bridge:
1. Set `B_EnemyGeneralController.BehaviorTree = BT_EnemyGeneral`.
2. PIE and verify: the general detects the hero, paths to within ~1.5 m, and its basic
   attack drops the hero's Health (read live via the GAS inspector).
3. Write `Done/DONE_enemy_general_ai.md` and close Phase 5.

(If you'd rather assign the BT yourself: open `B_EnemyGeneralController`, set its
`Behavior Tree` property to `BT_EnemyGeneral`, compile/save — then just PIE.)
```
