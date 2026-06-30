# DONE: Mass Soldier Combo Attacks (morale-driven) + Idle / Death Animation Fixes

## Summary

Phase 29. Two field requests for Mass soldiers:

1. **Morale-driven combo attacks.** A soldier's attack is now an *attack cycle* of **1–3 consecutive
   hits** instead of a single swing. The chance to extend a cycle scales linearly with the owning
   **general's morale (level 1–5)**: up to **10%** for a 3-hit and **20%** for a 2-hit at morale 5
   (e.g. morale 3 → 6% / 12%, morale 1 → 2% / 4%). Every soldier now has a **3-second** default attack
   cooldown, and after finishing a cycle it **returns its inner attack slot** so a waiting soldier
   strikes in its place (the attack ring rotates). The combo animations are authored in the unit's
   visual data as up to three montages.

2. **Idle / death animation fixes.** The "idle not playing" defect is an **AnimBP authoring** gap (the
   minion AnimBP needs an idle pose at `GroundSpeed≈0`), surfaced as an editor request. The death
   animation was a single-node `AnimSequence` while attack/hit were montages; for consistency it is now
   a **montage** (`DeathMontage`), with the held downed pose moved to an AnimBP **Dead state** driven by
   a new `bIsDead` flag.

`Attack_C` vs `Attack_C_SetB` (the user's question): these are two *variant* single-swing Paragon
minion attack clips (Epic ships an "A"/"B" set for crowd visual variety), **not** a built-in 3-chain —
they are used as building blocks for the combo montage array (e.g. hit 1 = `Attack_C`, hit 2 =
`Attack_C_SetB`).

## Files Changed

**Fragments / data**
- `Public/Mass/AshSoldierFragments.h` — `FAshCombatFragment` += `TwoHitChance`, `ThreeHitChance`,
  `ComboHitInterval`, `ComboLength`, `ComboHitsLanded`, `TimeSinceComboHit`; `FAshCombatEventFragment`
  += `AttackComboIndex`.
- `Public/Mass/AshMassSoldierConfig.h` — `AttackCooldown` default `1.5 → 3.0`; new `ComboHitInterval`.
- `Public/Mass/AshSoldierVisualConfig.h` — new `AttackComboMontages` array; `DeathAnim`
  (`UAnimSequenceBase`) → `DeathMontage` (`UAnimMontage`).
- `Public/Mass/AshSoldierAnimInstance.h` — new `bIsDead`.
- `Public/AI/AshGeneralConfig.h` — `InitialMoraleLevel`, `MaxMoraleLevel`, `MaxTwoHitComboChance`,
  `MaxThreeHitComboChance`.
- `Public/Mass/AshMassSoldierSpawnLibrary.h` / `Public/Mass/AshMassSoldierSpawner.h` — spawn-param combo
  chances + `FallbackComboHitInterval`; cooldown fallbacks `1.5 → 3.0`.

**Logic**
- `Private/Mass/AshMassCombatProcessor.cpp` — combo state machine (`AdvanceCombo` / `RollComboLength` /
  `EndCycle`) for both Mass-entity and Actor targets; per-hit damage + animation event; 3 s end-cycle
  cooldown.
- `Private/Mass/AshMassSoldierBehaviorProcessor.cpp` — `bActiveStriker` gate: a soldier on its
  post-cycle cooldown no longer holds an inner attack slot (both tally pass and decision pass, Mass +
  actor branches), so it yields to a `Surround` waiter.
- `Private/Mass/AshSoldierProxyActor.{h,cpp}` — `PlayAttackMontage(int32 ComboIndex)`; `PlayDeath()`
  plays `DeathMontage` + sets `bIsDead`; recycle clears `bIsDead` / stops montages (removed the
  single-node death path).
- `Private/Mass/AshMassRepresentationProcessor.cpp` — passes `AttackComboIndex` to the proxy.
- `Private/Mass/AshMassSoldierSpawnLibrary.cpp` — seeds the new combat-fragment fields.
- `Character/AshGeneralCharacter.{h,cpp}` — `MoraleLevel` + `GetMoraleLevel()` / `SetMoraleLevel()` /
  `ComputeComboChances()`; stamps chances at spawn and re-stamps live troops on a morale change.

## Implementation Details

- **Combo cycle.** `UAshMassCombatProcessor` advances each soldier's cooldown every frame. When idle and
  in range and off cooldown it rolls a combo length (`r < P3 → 3`, `r < P3+P2 → 2`, else `1`), lands hit
  1 and (for a multi-hit combo) keeps landing hits paced by `ComboHitInterval` until the chosen length
  is reached, then enters the next cooldown via `EndCycle`. Each hit applies `AttackPower` (a 3-hit
  cycle deals 3×) and raises the attacker's `bAttackedThisTick` with the `AttackComboIndex` so the
  representation layer plays the right montage. Losing the target mid-combo aborts the cycle into the
  cooldown.
- **Slot return.** The "attack slot" is the Phase 28 inner striking slot. A soldier is an *active
  striker* iff it is mid-combo or its cooldown has elapsed; the behavior processor counts inner-slot
  occupancy only for active strikers and only lets active strikers take/keep an inner seat. So a soldier
  on its 3 s post-cycle cooldown drops to `Surround` (keeping its target, ringing and menacing), freeing
  the slot for a waiting soldier — the ring rotates without interrupting an in-progress combo (mid-combo
  soldiers are always active strikers).
- **Morale → chances (data-oriented).** The general computes `chance = max * (MoraleLevel /
  MaxMoraleLevel)` and stamps `TwoHitChance` / `ThreeHitChance` onto each troop's combat fragment — no
  Mass→Actor reference in the hot loop. Stamped at spawn via the spawn params and re-stamped over
  `TroopEntities` on `SetMoraleLevel`.
- **Death montage.** Removing the single-node `PlayAnimation` path means the corpse pose is held by an
  AnimBP **Dead state** keyed on `bIsDead` (a montage blends out on its own). The proxy sets `bIsDead`
  with the montage and clears it on recycle so a pooled body animates live again.

## Architecture Notes

- All new tunables are data assets (`UAshMassSoldierConfig`, `UAshSoldierVisualConfig`,
  `UAshGeneralConfig`) — no hardcoded gameplay numbers (CLAUDE.md). Combo length, pacing, chances and
  the 3 s cooldown are all editable.
- Combat stays data-oriented: the combo runs entirely over Mass fragments in the combat/behavior
  processors; the proxy stays a brainless view driven by the one-shot event fragment.
- Morale lives on the high-fidelity general (Actor) and is pushed to the soldiers as plain fragment
  data, preserving the hierarchy (general decides, soldiers execute) and the no-strong-Mass→Actor rule.

## Testing / Verification

- Build verified: LyraEditor Win64 Development — **Result: Succeeded** (2026-06-29, editor closed; only
  the two pre-existing harmless warnings).
- PIE (PENDING USER): idle plays when soldiers stand; a soldier occasionally lands 2- or 3-hit combos
  (raise a `DA_General_*` `InitialMoraleLevel` to 5 for ~20% / 10%); each cycle is followed by a ~3 s
  pause during which a different soldier in the ring takes the attack slot; dead bodies play the death
  montage and hold the downed pose before despawn.

## Known Issues

- **AnimBP authoring is required (PENDING USER):** the minion AnimBP needs (1) an idle pose at
  `GroundSpeed≈0` and (2) a **Dead state** entered on `bIsDead` that holds the downed pose. Until the
  Dead state exists, the death montage will blend back to the locomotion pose at its end (regression
  from the Phase 27 single-node hold — the deliberate trade for montage consistency).
- Existing authored `DA_*` soldier configs keep their stored `AttackCooldown`; set them to `3.0`
  manually (the default only affects new/fallback assets).
- No morale gain/loss progression is implemented — `SetMoraleLevel()` is the seam for a future system.
- Plain `AAshMassSoldierSpawner` soldiers (no general) have 0 combo chance (single hits only) by design.

## Next Steps

- Author the minion AnimBP idle + Dead state and the per-unit combo/death montages, then PIE-verify.
- Optionally design a morale system (kills/objectives raise it, losses lower it) calling
  `AAshGeneralCharacter::SetMoraleLevel`.
