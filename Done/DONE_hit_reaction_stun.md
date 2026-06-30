# DONE: Hit-Reaction Stun + Knockback (Phase 32)

## Summary

Every combatant — the playable hero, the AI generals, and the Mass-entity soldiers — is now briefly
**stunned** and **pushed back ever so slightly** when it takes a melee hit. While stunned a unit can
neither move nor attack. The stun is a **GAS state**: ASC-owning characters receive `GE_State_Stunned`
(a Gameplay Effect granting the `Ash.State.Stunned` tag for a SetByCaller duration), and Mass soldiers
carry the analogous `FAshStunFragment`.

A **game-wide stun rule** prevents infinite stun-locking: after being stunned, a *different* attacker
cannot stun the victim again until `UAshCombatRulesSettings::NewStunSourceImmunity` (default 2 s) elapses,
while the *same* attacker's continuous combo may always re-stun. This guarantees a counterattack window.

## Files Changed

- `Public/AbilitySystem/AshGameplayEffect_Stun.h` / `.cpp` — **new** `GE_State_Stunned`: HasDuration,
  duration via `Ash.Data.StunDuration` SetByCaller, grants `Ash.State.Stunned` via
  `UTargetTagsGameplayEffectComponent`.
- `Public/Combat/AshCombatRulesSettings.h` / `.cpp` — **new** `UAshCombatRulesSettings` (UDeveloperSettings;
  `NewStunSourceImmunity`) + the shared `AshCombat::ApplySoldierStun` rule helper.
- `Public/AshGameplayTags.h` / `.cpp` — new `Ash.Data.StunDuration` tag.
- `Public/Mass/AshSoldierFragments.h` — new `FAshStunFragment` (StunTimeRemaining / StunDuration /
  KnockbackVelocity / LastStunSourceId / LastStunTime).
- `Private/Mass/AshMassSoldierSpawnLibrary.cpp` — added `FAshStunFragment` to the soldier archetype.
- `Public/Mass/AshSoldierBehaviorConfig.h` — new `StunDuration` / `KnockbackSpeed` tunables (HitReact).
- `Private/Mass/AshMassMovementProcessor.cpp` (+ `.h` query) — owns the stun countdown, applies the
  decaying knockback slide, suppresses steering while stunned.
- `Private/Mass/AshMassCombatProcessor.cpp` (+ `.h` query) — skips striking while stunned; on landing a
  hit, stuns the Mass victim (`AshCombat::ApplySoldierStun`) and calls `ApplyHitReaction` on a struck
  hero/general; passes the attacker's entity index as the stun source id.
- `Private/Mass/AshSoldierProxyActor.cpp` — `ReceiveMeleeHit` calls the shared helper (instigator UniqueID
  as source id).
- `Public/Teams/AshTeamAgentInterface.h` — new `ApplyHitReaction(const FVector& SourceLocation, int32
  SourceId)` (default no-op; hero/general override).
- `Public/Character/AshHeroCharacter.h` / `.cpp` — `ApplyHitReaction`, `ApplyStun` (GE-based + immunity
  rule), `IsStunned` (queries the tag), `StunEffectClass`, `HitReact*`, `LastStunSourceId/Time`; movement
  input gated while stunned.
- `Public/Character/AshGeneralCharacter.h` / `.cpp` — same stun surface for the general.
- `Private/AI/StateTree/AshGeneralStateTreeNodes.cpp` — each task freezes movement/orders while
  `IsStunned()` (shared `HandleStunned` helper).
- `Private/AbilitySystem/Abilities/AshGA_BasicAttack.cpp` — `ActivationBlockedTags` = {Stunned, Dead};
  `PerformHitSweep` applies the hit reaction to ASC targets.

## Implementation Details

- **GAS characters (hero / general).** On being hit, `ApplyHitReaction(SourceLocation, SourceId)` launches
  the character back from the attacker (`LaunchCharacter`, speed `HitReactKnockbackSpeed`) and calls
  `ApplyStun(HitReactStunDuration, SourceId)`. `ApplyStun` first runs the **new-source immunity rule**
  (below); when allowed it builds a spec from `StunEffectClass` (default `GE_State_Stunned`), sets the
  `Ash.Data.StunDuration` SetByCaller, applies it to self, and cancels in-flight abilities (the general
  also stops its AI move). The GE grants `Ash.State.Stunned` for its lifetime and removes it on expiry —
  no manual timer. `IsStunned()` simply queries the tag. Attacks are blocked because `UAshGA_BasicAttack`
  lists `Ash.State.Stunned` in `ActivationBlockedTags`; the hero's movement input is gated on the tag, and
  the general's StateTree tasks early-out (StopMovement) while `IsStunned()`.
- **Mass soldiers.** With no ASC, a soldier carries `FAshStunFragment`. The movement processor decrements
  the timer, slides the soldier under a knockback that eases to zero over the window
  (`KnockbackVelocity * remaining/duration`), and `continue`s past all steering while stunned. The combat
  processor `continue`s past striking while `StunTimeRemaining > 0`. The stun is applied through the shared
  `AshCombat::ApplySoldierStun` at both damage entry points (proxy `ReceiveMeleeHit`; combat processor
  `LandHit`), reading the victim's own `UAshSoldierBehaviorConfig` and honoring the same immunity rule.
- **New-source stun immunity (game-wide rule).** `UAshCombatRulesSettings` is a `UDeveloperSettings`
  (Project Settings → Game → "Ash Combat Rules"), so the rule is globally reachable and data-editable and
  the asset is the home for future cross-cutting combat rules. The rule: a stun from source `S` is allowed
  if `S` is the same source that last stunned the victim (a continuous combo — always allowed) **or** at
  least `NewStunSourceImmunity` seconds have passed since the last stun. Each victim tracks its last stun
  source + time (hero/general fields; `FAshStunFragment::LastStunSourceId/LastStunTime`). Knockback is not
  gated (it never blocks input), so the protected window is genuinely free for a counterattack.
- **Stun duration is data, ready for weapon/skill variation.** Today the duration is the victim's
  `HitReactStunDuration` (per-character) / `UAshSoldierBehaviorConfig::StunDuration` (per-unit), fed into
  the GE via SetByCaller. Later a weapon/skill can supply a different `StunEffectClass` or duration with no
  code change.

## Architecture Notes

- Stun is the `Ash.State.Stunned` gameplay tag (ARCHITECTURE.md 4.4), now granted by a first-class
  Gameplay Effect (ARCHITECTURE.md 5.3 `GE_State_Stunned`). Soldiers stay data-oriented (a Mass fragment,
  processor-owned countdown) rather than gaining an ASC, per ARCHITECTURE.md 7. No new Actor Tick (18.3):
  the GE owns its own duration; soldiers use the existing per-frame movement processor.
- The reaction is applied at the existing authoritative damage choke points, keeping it server-authority
  friendly (5.5 / 15). `IAshTeamAgentInterface::ApplyHitReaction` keeps the attacker decoupled from
  concrete victim classes (18.4). Tunables live in data assets / developer settings (17).

## Testing / Verification

- PENDING USER (build editor-closed, then PIE): strike a soldier — it flinches back and briefly stops;
  strike a general — it is shoved + can't move/attack for the window; one enemy comboing the hero
  stun-locks for that combo, but a second enemy cannot add a stun for `NewStunSourceImmunity` seconds —
  giving a clean counterattack window. Tune `Project Settings → Game → Ash Combat Rules`, the
  `DA_*_Behavior` StunDuration/KnockbackSpeed, and the hero/general `HitReact*`.

## Known Issues

- A single enemy's continuous combo will stun-lock the victim for that combo (intended/unavoidable by
  design); only *new* sources are gated. Tune the per-character/unit `StunDuration` if a combo lock feels
  too long.
- The general's knockback can be slightly damped by its StateTree `StopMovement` call on the same frame;
  the shove still reads but is brief.

## Next Steps

- Optional: a dedicated hit-react/stagger montage driven by the `Ash.State.Stunned` tag for the
  hero/general (the soldiers already play their hit-react montage via the existing combat event).
- Weapon/skill-specific stun: author a `GE_State_Stunned` variant (or vary the SetByCaller duration) per
  weapon and assign via `StunEffectClass`.
