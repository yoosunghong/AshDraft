# DONE: GAS Combat Foundation (Phase 4)

## Summary

Implemented the Gameplay Ability System (GAS) combat foundation for AshDraft
(ARCHITECTURE.md section 5). Added an ability system component wrapper, the
initial attribute set (Health/MaxHealth/AttackPower/Defense/Stamina/MaxStamina),
a data-driven damage Gameplay Effect with an AttackPower-vs-Defense execution
calculation, a base Gameplay Ability class, and a basic melee attack ability.
The whole damage path is wired into `AAshHeroCharacter`, and the Phase 3
placeholder Health/MaxHealth floats were migrated onto the attribute set.

The full chain is now in code: **Enhanced Input → ability activation →
damage Gameplay Effect → execution calculation → target Health → death tag.**

## Files Changed

New (C++):
- `Source/AshDraftCoreRuntime/Public/AbilitySystem/AshAbilitySystemComponent.h`
  / `Private/.../AshAbilitySystemComponent.cpp` — `UAshAbilitySystemComponent`
  wrapper with input-tag → ability activation routing.
- `Public/AbilitySystem/AshAttributeSet.h` / `Private/.../AshAttributeSet.cpp` —
  `UAshAttributeSet` (the six attributes + a transient `Damage` meta attribute,
  clamping, damage application, death tagging).
- `Public/AbilitySystem/AshGameplayAbility.h` / `Private/.../AshGameplayAbility.cpp`
  — `UAshGameplayAbility` base (carries an `InputTag`, sane instancing/net policy).
- `Public/AbilitySystem/AshDamageExecution.h` / `Private/.../AshDamageExecution.cpp`
  — `UAshDamageExecution` (`GameplayEffectExecutionCalculation`): captures source
  AttackPower + target Defense, outputs mitigated damage into `Damage`.
- `Public/AbilitySystem/AshGameplayEffect_Damage.h` /
  `Private/.../AshGameplayEffect_Damage.cpp` — `UAshGameplayEffect_Damage`
  (instant GE configured in C++ to run the execution; the GE_Damage_Physical of 5.3).
- `Public/AbilitySystem/Abilities/AshGA_BasicAttack.h` /
  `Private/.../AshGA_BasicAttack.cpp` — `UAshGA_BasicAttack` (plays the ability
  montage, sweeps a sphere on the hit notify, applies the damage GE to hit ASC
  owners; instant-sweep fallback when no montage is set).
- `Public/Animation/AshAnimNotify_MeleeHit.h` / `Private/.../AshAnimNotify_MeleeHit.cpp`
  — `UAshAnimNotify_MeleeHit` ("Ash Melee Hit") montage notify that sends the
  `Ash.Event.Hit.Melee` gameplay event at the contact frame.

Modified:
- `Source/AshDraftCoreRuntime/AshDraftCoreRuntime.Build.cs` — added
  `GameplayAbilities` + `GameplayTasks` public dependencies.
- `AshDraftCore.uplugin` — added the `GameplayAbilities` plugin dependency.
- `Public/AshGameplayTags.h` / `Private/AshGameplayTags.cpp` — added `Ash.Data.Damage`.
- `Public/Character/AshHeroCharacter.h` / `Private/Character/AshHeroCharacter.cpp`
  — hero implements `IAbilitySystemInterface`, owns the ASC + attribute set,
  initializes actor info, seeds attributes from editor-tunable initial stats,
  grants `DefaultAbilities`, binds ability input actions by tag, and reads health
  from the attribute set (placeholder floats removed).
- `Docs/PLAN.md`, `Docs/Plan/Phase4.md` — progress updated.

## Implementation Details

- **ASC on the avatar.** For the single-player PoC the `UAshAbilitySystemComponent`
  is a subobject of `AAshHeroCharacter` (the GAS sample / ARPG pattern), not on a
  PlayerState. It is replicated in Mixed mode so the shape stays
  server-authority-friendly (ARCHITECTURE.md 5.5 / 15). `InitAbilityActorInfo`
  runs on both `PossessedBy` (authority) and `PawnClientRestart` (owning client);
  attribute seeding + ability granting are authority-gated and guarded against
  double-granting.
- **Attribute set + meta damage.** Health changes are funneled through a transient
  `Damage` meta attribute in `PostGameplayEffectExecute`, keeping all damage on one
  authoritative path rather than letting effects poke Health directly. Reaching
  zero health adds the loose `Ash.State.Dead` tag (event-driven death, 4.4 / 15).
  `PreAttributeChange` / `PreAttributeBaseChange` clamp current-vs-max pairs.
- **Damage via execution calculation.** `UAshGameplayEffect_Damage` is an *instant*
  effect that runs `UAshDamageExecution`, computing
  `max(AttackPower - Defense, MinDamage)`. This keeps the GE itself pure data while
  the formula stays authoritative and tunable — no flat magnitudes baked into the
  GE. The GE is configured in C++ so the foundation is verifiable without an
  editor-authored asset; a Blueprint child can override it later.
- **Input-to-ability (Lyra-style).** Abilities derive from `UAshGameplayAbility`
  and expose an `InputTag`. On grant, the hero stamps that tag onto the ability
  spec's dynamic tags. `SetupPlayerInputComponent` binds every
  `UAshInputConfig::AbilityInputActions` entry to `Input_AbilityTagPressed/Released`,
  which forwards to the ASC, which activates the matching spec(s). The Phase 1
  `Ash.InputTag.Attack.*` / `Dodge` tags now have a live consumer.
- **Basic attack.** `UAshGA_BasicAttack` (input tag `Ash.InputTag.Attack.Basic`)
  sweeps a sphere in front of the avatar and applies the damage GE once per
  distinct hit ASC, skipping itself. Instant hit, no montage/combo — deliberately
  minimal for the foundation. It adds `Ash.State.Attacking` via `ActivationOwnedTags`.

## Animation / Montage Structure

The canonical place to set an ability's animation is **a montage property on the
ability**, assigned in a `GA_*` Blueprint child (data-driven, ARCHITECTURE.md 17):

- `UAshGameplayAbility` (base) exposes `AbilityMontage` + `AbilityMontagePlayRate`
  — the single-montage slot used by simpler abilities (heavy attack / dodge later).
- `UAshAnimNotify_MeleeHit` ("Ash Melee Hit") is the notify placed on a montage at
  the contact frame; it sends `Ash.Event.Hit.Melee`, which the active ability
  listens for to run its damage sweep — hit timing is authored in the animation.

### Basic attack: 4-hit combo with notify-driven cancel window

`UAshGA_BasicAttack` is a combo ability driven by an ordered montage array, using
an animation-authored **cancel window** for responsiveness (no fixed recovery wait):

- `ComboMontages` — ordered `TArray<UAnimMontage*>` (PoC uses 4); `ComboMontagePlayRate`.
- The ability is **one activation for the whole combo** (`InstancedPerActor`).
  Step 0 plays on the first press.
- Each montage carries a `UAshAnimNotifyState_ComboWindow`. While that window is
  open, a repeat press **cancels the current montage immediately and plays the
  next** (`AdvanceCombo` → `PlayComboMontage`, which interrupts the old animation).
  This removes the slow recovery "after delay" the previous chain-on-completion
  model had. A press before the window (wind-up) is **buffered** and fires the
  instant the window opens. If a montage plays to its end uncancelled, the combo
  ends (next press restarts at step 0).
- Self-cancel safety: before starting the next montage, `StopCurrentMontageTask()`
  detaches the old `PlayMontageAndWait` task (RemoveDynamic + EndTask) so its
  interrupt callback doesn't end the whole ability. Only *external* interrupts
  reach `OnComboMontageInterrupted`.
- Input buffering plumbing: while the attack ability is active, the ASC
  (`UAshAbilitySystemComponent::AbilityInputTagPressed`) forwards a repeat attack
  press as the `Ash.Event.Combo.Next` gameplay event instead of re-activating; the
  ability waits on it (`UAbilityTask_WaitGameplayEvent`). The first press (ability
  inactive) activates normally. Window open/close arrive as
  `Ash.Event.Combo.WindowOpen/Close` from the notify state.
- Damage: each combo montage carries its own `Ash Melee Hit` notify; a single
  persistent `WaitGameplayEvent(Ash.Event.Hit.Melee)` sweeps per hit.
- Fallback: with `ComboMontages` empty it does one instant sweep and ends.

**Where to set the basic attack animations:** open
`/AshDraftCore/Abilities/GA_Hero_BasicAttack`, fill `ComboMontages` with the 4
attack montages in order. On each montage place:
- an **`Ash Melee Hit`** notify at the contact frame (damage), and
- an **`Ash Combo Window`** notify state spanning from ~the contact frame to the
  end of the montage (the cancellable region — start it at/just after the Ash Melee
  Hit so the hit always lands before a cancel).

Raise `ComboMontagePlayRate` to speed every hit up. (Requires recompiling the
module so the new properties/notifies show.)

## Editor Wiring (this session, via unreal-mcp)

- Created `/AshDraftCore/Abilities/GA_Hero_BasicAttack` (Blueprint child of
  `UAshGA_BasicAttack`) — the home for the basic-attack montage.
- `B_Hero_Ash.DefaultAbilities = [GA_Hero_BasicAttack]`.
- Confirmed `DA_InputConfig_AshDefault.AbilityInputActions` already maps
  `IA_AttackBasic → Ash.InputTag.Attack.Basic` (+ Heavy/Dodge), and the hero's
  `InputConfig` / `DefaultMappingContext` are set; `DA_PawnData_AshHero.PawnClass`
  is `B_Hero_Ash`. Assets saved.

## Architecture Notes

- Follows ARCHITECTURE.md 5 end to end: attribute set (5.2), damage GE (5.3),
  basic attack ability (5.4), authority-minded design (5.5).
- Data-driven tuning (17): initial stats are `EditAnywhere` on the hero and pushed
  into attribute base values; abilities/effects are `TSubclassOf` properties.
- No Actor Tick (18.3); everything is GAS/event-driven. Naming follows the `Ash`
  prefix and one-class-per-file rules (18.1 / 18.2).
- No Lyra core edits; the plugin owns its GAS types. New module deps
  (`GameplayAbilities`, `GameplayTasks`) added to Build.cs + the plugin descriptor.

## Testing / Verification

Not compiled/run here (no live editor build in this environment). To verify:

1. Compile `AshDraftCoreRuntime` (Live Coding or full build). Confirm the
   `GameplayAbilities` plugin is enabled (it is for Lyra projects; the descriptor
   now also lists it).
2. On `AAshHeroCharacter` (or a `B_Hero_Ash` Blueprint), set `DefaultAbilities` to
   include `UAshGA_BasicAttack`.
3. In `DA_InputConfig_AshDefault` (`UAshInputConfig`), add an `AbilityInputActions`
   entry mapping the attack `UInputAction` to `Ash.InputTag.Attack.Basic`.
4. Repoint `DA_PawnData_AshHero.PawnClass` to the Ash hero if not already.
5. PIE: place a second pawn with an ASC + `UAshAttributeSet` (e.g. a duplicate
   hero) in front of the player; press the attack input and confirm the target's
   `Health` drops (`showdebug abilitysystem`) and `Ash.State.Dead` is applied at 0.

## Known Issues

- **Editor wiring pending.** `DefaultAbilities` and the input config's
  `AbilityInputActions` are empty by default — they must be populated in-editor
  (or via the MCP bridge) before the attack fires. The "verify damage in PIE"
  checklist item stays unchecked until a live build confirms it.
- **No attack montage / combo / hit reaction.** The basic attack is an instant
  sphere sweep; animation timing, combo windows, and hit reactions are later
  combat phases.
- **No target acquisition for AI.** Only the player hero is wired; the enemy
  general and its ASC arrive in Phase 5, which provides a real damage target.
- **`GetDynamicSpecSourceTags()`** is used for spec tag access (UE 5.5+ API). On an
  older engine, swap to `Spec.DynamicAbilityTags`.
- **Heavy attack / dodge** abilities and the heal/stun/invincible effects from
  ARCHITECTURE.md 5.3–5.4 are not implemented yet; only the basic-attack/damage
  slice required by Phase 4 is done.

## Next Steps

- Compile, populate `DefaultAbilities` + `AbilityInputActions`, and verify damage
  in PIE (the remaining Phase 4 checklist item).
- Phase 5: Enemy General Character and AI — implement `AAshEnemyGeneralCharacter`
  with its own ASC/attribute set (the first real damage target), then layer the
  AIController/Behavior Tree.
- Later combat passes: `GA_Hero_HeavyAttack`, `GA_Hero_Dodge`, attack montages and
  hit reactions, and the remaining state effects (stun/invincible/heal).
