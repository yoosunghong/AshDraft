# DONE: Hero Attack Improvements (Phase 32)

## Summary

Three combat-feel improvements to the hero's basic attack:

1. The hero **steps forward slightly** during each attack (forward lunge).
2. Attacking while moving now **stops the hero first** — the character plants and commits to the strike
   instead of sliding through it. Movement is blocked for the duration of the attack.
3. Pressing attack after **moving continuously for 2+ seconds** plays a **Dash Attack** montage instead of
   the normal combo opener.

## Files Changed

- `Public/AbilitySystem/Abilities/AshGA_BasicAttack.h` — `DashAttackMontage`, `AttackLungeSpeed`,
  `DashAttackLungeSpeed`, `bDashAttack`.
- `Private/AbilitySystem/Abilities/AshGA_BasicAttack.cpp` — stop-on-activate, dash detection, dash-opener
  montage selection, per-hit forward lunge.
- `Public/Character/AshHeroCharacter.h` / `.cpp` — `ShouldUseDashAttack()` + continuous-move tracking
  (`MoveInputStartTime`/`LastMoveInputTime`, `DashAttackMoveSeconds`, `MoveInputResetGap`); movement input
  blocked while `Ash.State.Attacking`.

## Implementation Details

- **Forward lunge.** `PlayComboMontage` launches the avatar forward (`LaunchCharacter`,
  `GetActorForwardVector() * AttackLungeSpeed`) as each hit begins; the dash opener uses the larger
  `DashAttackLungeSpeed`. Defaults raised to 500 / 1000 cm/s (the original 250 produced only a ~16 cm step
  under the hero's 2000 braking deceleration — effectively invisible). 0 disables it. Applied generically
  to any `ACharacter` attacker. **Caveat:** an attack montage with in-place root motion overrides the
  lunge — disable the montage's root motion (or author it to translate forward) for the step to show.
- **Stop moving to attack.** `ActivateAbility` calls `StopMovementImmediately()` on the avatar's movement
  component, and `AAshHeroCharacter::Input_Move` returns early while the ASC holds `Ash.State.Attacking`
  (the ability's activation-owned tag). So the hero plants for the whole combo; the lunge provides the
  forward step. Movement resumes the instant the combo ends (tag removed).
- **Dash attack (fixed).** `AAshHeroCharacter` records when continuous movement began (`Input_Move` resets
  the run timer after a gap of `MoveInputResetGap` without input). `ShouldUseDashAttack()` returns true
  when the hero is moving now (recent input + real velocity) and the run length ≥ `DashAttackMoveSeconds`
  (default 2 s). **Bug fix:** the dash check is now evaluated *before* `StopMovementImmediately()` in
  `ActivateAbility` — previously the stop had already zeroed the velocity `ShouldUseDashAttack()` reads, so
  it never triggered. When true and a `DashAttackMontage` is assigned, it plays as the opening hit; a press
  inside its cancel window chains into `ComboMontages[1]`.
- **Every combo hit deals damage (fixed).** Combo steps 3 & 4 previously dealt no damage when their
  montages lacked an Ash Melee Hit notify. Each step now tracks `bStepSwept`: the notify sets it, and if a
  step is left (cancelled into the next via `AdvanceCombo`, or the final montage ends) without having
  swept, the ability performs one fallback `PerformHitSweep()`. So every hit lands exactly once regardless
  of per-montage notify authoring.

## Architecture Notes

- All magnitudes are editor-tunable UPROPERTYs (CLAUDE.md: no hardcoded gameplay numbers). The dash montage
  is data assigned on the `GA_BasicAttack` Blueprint (ARCHITECTURE.md 5.4 / 17). No new Actor Tick — the
  run timer is updated only inside the existing Enhanced-Input handler. The attack ability stays the shared
  hero/general path; the dash-attack branch is hero-only (gated by the `AAshHeroCharacter` cast).

## Testing / Verification

- PENDING USER (build editor-closed, then editor + PIE): assign a `DashAttackMontage` (with the Ash Melee
  Hit notify) on the hero's `GA_BasicAttack`; tune `AttackLungeSpeed` / `DashAttackLungeSpeed`. PIE-verify:
  each swing steps forward; a full 4-hit combo all deals damage; attacking while running stops the hero (no
  sliding); running for 2 s+ then attacking plays the dash attack.

## Known Issues

- With no `DashAttackMontage` assigned, a 2 s+ run still opens with the normal combo (graceful fallback).
- Movement is fully locked during the combo (matches the "stop to attack" request); if a more
  move-cancellable feel is wanted later, relax the `Ash.State.Attacking` gate in `Input_Move`.

## Next Steps

- Optional: a dedicated heavy/dash-attack ability (`GA_Hero_HeavyAttack`) sharing this lunge/dash plumbing.
- Optional: root-motion-driven lunge instead of `LaunchCharacter` once attack montages carry root motion.
