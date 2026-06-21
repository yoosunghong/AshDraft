# DONE: Enemy General Character and AI (Phase 5)

## Summary

Implemented the high-importance enemy general (ARCHITECTURE.md 6.2): a Character-based,
GAS-enabled combatant driven by a Behavior Tree. It detects the player, chases over the
NavMesh, and executes the shared basic-attack ability, with event-driven hit/death
handling and a morale placeholder. Verified in PIE — the general successfully chased and
attacked the hero.

## Files Changed

C++ (`Source/AshDraftCoreRuntime/`):
- `Public/Character/AshEnemyGeneralCharacter.h` + `Private/Character/AshEnemyGeneralCharacter.cpp` (new)
- `Public/AI/AshEnemyGeneralController.h` + `Private/AI/AshEnemyGeneralController.cpp` (new)
- `Public/AI/AshBTService_DetectPlayer.h` + `Private/AI/AshBTService_DetectPlayer.cpp` (new)
- `Public/AI/AshBTTask_ActivateAbilityByInputTag.h` + `Private/AI/AshBTTask_ActivateAbilityByInputTag.cpp` (new)
- `AshDraftCoreRuntime.Build.cs` — added `AIModule`, `NavigationSystem`

Assets (`/AshDraftCore/`):
- `Game/Character/B_EnemyGeneral` — Blueprint child of `AAshEnemyGeneralCharacter`;
  `DefaultAbilities = [GA_Hero_BasicAttack]`, `AIControllerClass = B_EnemyGeneralController`.
- `AI/B_EnemyGeneralController` — Blueprint child of `AAshEnemyGeneralController`;
  `BehaviorTree = BT_EnemyGeneral`.
- `AI/BB/BB_EnemyGeneral` — keys `TargetActor` (Object/Actor), `TargetDistance` (Float).
- `AI/BT/BT_EnemyGeneral` — Selector + `Ash Detect Player` service + `TargetActor IsSet`-guarded
  Sequence [MoveTo(150) → `Ash Activate Ability (Input Tag)` → Wait].
- `Maps/BaseLevel` — placed `B_EnemyGeneral`, added `NavMeshBoundsVolume` (+ RecastNavMesh).

Docs: `Docs/Guides/Phase5_BehaviorTree_Setup.md` (BB/BT authoring guide).

## Implementation Details

- **GAS reuse:** the general mirrors the hero's GAS init (`UAshAbilitySystemComponent` +
  `UAshAttributeSet`, attributes seeded in `InitializeAttributes`, abilities granted with
  their input tag stamped on the spec). It is granted the **same** `UAshGA_BasicAttack` as
  the hero rather than a separate AI ability.
- **AI triggers the attack through the player's input path:** `UAshBTTask_ActivateAbilityByInputTag`
  calls `UAshAbilitySystemComponent::AbilityInputTagPressed(InputTag_Attack_Basic)`. This is
  why abilities are granted with the input tag in their spec dynamic tags.
- **Detection:** `UAshBTService_DetectPlayer` ticks on the Selector, locates the local player
  pawn, writes `TargetActor`/`TargetDistance`, sets controller focus (so the general faces the
  player before swinging), and uses sight/lose-sight hysteresis to avoid edge flicker.
- **Approach:** built-in BT MoveTo to `TargetActor` over the NavMesh.
- **Hit/death:** Health-change delegate → `HandleDeath()` stops the BT, cancels abilities,
  disables collision/movement, broadcasts `OnGeneralDied`, and applies `Ash.State.Dead`.
- **Morale:** `CurrentMorale` scalar placeholder; no behavior wired yet.
- No Actor Tick — controller/character are BT/GAS/delegate-driven (ARCHITECTURE.md 18.3).

## Architecture Notes

- Character (not Mass Entity) for the general — high-fidelity decisions, animation, direct
  combat (ARCHITECTURE.md 6.2). Hierarchical-AI ready: this is the thin "general reacts to
  player" layer; squad/commander coordination layers on later without a UI dependency
  (16 / 18.4).
- Authority-friendly: ASC replicated, damage/death routed through the GE pipeline and tags
  (5.5 / 15). Tunables (health, attack range, morale, sight radii) are data-driven (17).
- Reuses the Phase 4 GAS foundation and the shared basic-attack ability rather than
  duplicating combat logic.

## Testing / Verification

Verified in PIE (2026-06-21, user-run): with `BT_EnemyGeneral` assigned to
`B_EnemyGeneralController`, the placed `B_EnemyGeneral` detected the player, chased over the
RecastNavMesh, and executed its basic attack against the hero. Asset existence and Blueprint
compiles confirmed over the MCP bridge.

## Known Issues

- Enemy general ships without a custom mesh/anim on the Blueprint (functional capsule +
  whatever the BP author assigned); polish is cosmetic.
- The hero/general GAS init is duplicated between `AAshHeroCharacter` and
  `AAshEnemyGeneralCharacter`; a shared `AAshCombatCharacter` base could de-duplicate it in a
  later refactor (kept separate now for PoC velocity, ARCHITECTURE.md 19).
- Morale is a placeholder with no behavior; retreat/regroup logic is a later phase.
- Detection is single-player via `GetPlayerPawn(0)`; multi-target needs a perception/team-query
  detector.

## Next Steps

- Phase 6: 8-Direction Combat Slot System (`UAshCombatSlotComponent`) — reusable slot
  management around high-value targets like the general and the player.
