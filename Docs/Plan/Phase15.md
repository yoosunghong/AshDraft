# Phase 15: Battlefield PoC Map

Goal: Build the first actual test battlefield.

- [ ] Create one PoC battlefield map
- [ ] Add ally base
- [ ] Add central neutral base
- [ ] Add enemy base
- [ ] Add 2-3 main lanes
- [ ] Add one chokepoint
- [ ] Add one wide battle area
- [ ] Add player spawn
- [ ] Add enemy general spawn
- [ ] Add soldier spawn points
- [ ] Verify basic match flow
- [ ] Create `Done/DONE_battlefield_poc_map.md`

## Added work: Mass soldier skeletal meshes + animations (data-driven unit types)

Goal: rendered Mass soldiers are real skeletal-mesh characters that animate, not debug
points/static meshes — and a unit's whole look is data-driven so multiple unit types are
authored as data, not Blueprints/code. `B_Soldier_Proxy` is one generic empty template; its
mesh/AnimBP/montages come from a `UAshSoldierVisualConfig` applied at runtime. Combat events
are surfaced from the Mass combat processor to the proxy so it can react.

- [x] `UAshSoldierVisualConfig` data asset (skeletal mesh, AnimBP, mesh transform, attack/hit montages)
- [x] `UAshMassSoldierConfig` becomes the unit-type asset: stats + a `Visual` reference
- [x] `AAshSoldierProxyActor` → `USkeletalMeshComponent`; `ConfigureVisual()` dresses it at runtime
      (idempotent), orient to movement; montages read from the current visual set
- [x] `FAshVisualFragment` (per-entity visual-set link) + `FAshCombatEventFragment` (one-shot
      attack/hit flags) added to the soldier archetype and seeded by the spawner
- [x] Combat processor sets the attacker's attack flag and the victim's hit flag when a strike lands
- [x] Representation processor configures the proxy's visual, plays attack/hit montages, then clears
      the flags each frame (runs after the combat processor)
- [x] Create `Done/DONE_mass_soldier_skeletal_anim.md`
- [ ] EDITOR (user): keep `B_Soldier_Proxy`'s mesh component empty; author `DA_<Unit>_Visual`,
      reference it from `DA_MassSoldierConfig.Visual`; PIE-verify attack/hit animations. Duplicate
      the assets for additional unit types reusing the same proxy class.

## Added work: editor-configurable victory / defeat conditions

Goal: designers choose win/lose conditions per level in the editor instead of the conditions
being hardcoded in `UAshMatchSubsystem`. Conditions stay event-driven and GAS-backed
(hero/general death already flow from the GAS attribute set).

- [x] `UAshMatchRulesConfig` data asset: toggle victory (all bases captured, enemy generals
      eliminated) and defeat (hero death, main base lost) conditions + optional time limit/outcome
- [x] `AAshMatchRulesActor` placeable holder that carries the rules asset for a level
- [x] `UAshMatchSubsystem` reads the level's rules (sensible defaults when none placed),
      gates every end condition on its flags, binds enemy-general deaths, and runs the time limit
- [x] New end reasons (`EnemyGeneralsEliminated`, `TimeLimitReached`) + `EAshTimeLimitOutcome`
- [x] Create `Done/DONE_editor_match_rules.md`
- [ ] EDITOR (user): create a `DA_MatchRules`, drop an `AshMatchRulesActor` in the level and
      pick conditions; PIE-verify each selected condition fires