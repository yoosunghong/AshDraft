# Phase 4: GAS Combat Foundation

Goal: Implement the basic GAS-based combat foundation.

- [x] Create AshDraft Ability System Component wrapper if needed
- [x] Create initial Attribute Set
- [x] Add Health attribute
- [x] Add AttackPower attribute
- [x] Add Defense attribute
- [x] Add Stamina attribute
- [x] Create basic damage Gameplay Effect
- [x] Create basic attack Gameplay Ability
- [x] Bind attack input to ability activation
- [x] Verify that a target can receive damage
  - Wiring done (via unreal-mcp): `GA_Hero_BasicAttack` Blueprint (child of
    `UAshGA_BasicAttack`) created at `/AshDraftCore/Abilities/`, added to
    `B_Hero_Ash.DefaultAbilities`; input config's `AbilityInputActions` maps
    `IA_AttackBasic → Ash.InputTag.Attack.Basic`; `DA_PawnData_AshHero.PawnClass`
    points at `B_Hero_Ash`.
  - Verification target: `AAshGASTargetDummy` (Lyra B_TargetDummy-style) with a
    Pawn-channel capsule + ASC; a `B_Dummy` Blueprint child is placed in `BaseLevel`.
  - CONFIRMED in PIE (2026-06-21): `LogAshGASVerify` recorded the dummy dropping
    100 → 75 → 50 → 25 → 0 (−25 per hit = AttackPower 25 − Defense 0), reaching zero
    and tagging `Ash.State.Dead`. Damage pipeline validated end-to-end.
- [x] Create `Done/DONE_gas_combat_foundation.md`