# DONE: GAS Damage Verification Target

## Summary

Added `AAshGASTargetDummy`, a minimal GAS-enabled training/verification target, to
make the Phase 4 task "verify that a target can receive damage" actually testable.
The hero's basic attack already applies a damage Gameplay Effect to any hit actor
that owns an ability system, but the level had no second ASC owner to hit (the real
enemy general is scoped to Phase 5). The dummy fills that gap without pulling Phase 5
work forward: it owns a `UAshAbilitySystemComponent` + `UAshAttributeSet`, exposes a
Pawn-channel collision capsule the melee sweep can register, and reports every Health
change to the log and the on-screen HUD so the damage drop is observable in PIE.

## Files Changed

- `Source/AshDraftCoreRuntime/Public/Verification/AshGASTargetDummy.h` (new)
- `Source/AshDraftCoreRuntime/Private/Verification/AshGASTargetDummy.cpp` (new)
- `Docs/Plan/Phase4.md` (note under the "Verify that a target can receive damage" task)

No Build.cs change was needed — `GameplayAbilities`, `GameplayTags`, and `Engine`
were already in the module dependencies.

## Implementation Details

- **Collision:** root `UCapsuleComponent` set to object type `ECC_Pawn` and blocking
  all channels. This is the key detail: `UAshGA_BasicAttack::PerformHitSweep()` uses
  `SweepMultiByChannel(..., ECC_Pawn, ...)`, so the target must respond to the Pawn
  trace channel to be registered as a hit.
- **GAS:** mirrors the hero's setup — ASC created in the constructor, replicated, Mixed
  replication mode; `InitAbilityActorInfo(this, this)` in `BeginPlay`; attribute base
  values (`MaxHealth`/`Health`/`Defense`) seeded on authority via
  `SetNumericAttributeBase`. Implements `IAbilitySystemInterface` so
  `UAbilitySystemGlobals::GetAbilitySystemComponentFromActor` finds the ASC during the
  sweep.
- **Observability:** binds to the Health attribute-change delegate
  (`GetGameplayAttributeValueChangeDelegate`) and, on each change, writes a log line
  (`LogAshGASVerify`) and an `AddOnScreenDebugMessage` (yellow, red at zero). The
  delegate is unbound in `EndPlay`.
- **Defaults:** `InitialDefense = 0` so the full hit lands and the change is obvious;
  `TeamId = Enemy` so it reads as a valid hostile target. Both are editor-tunable.
- **No Tick:** `PrimaryActorTick.bCanEverTick = false` (ARCHITECTURE.md 18.3); all
  behavior is delegate-driven.

## Architecture Notes

- Keeps GAS as the single authoritative damage path (ARCHITECTURE.md 5.5): the dummy
  changes Health only through the same Gameplay Effect pipeline the hero applies; death
  tagging (`Ash.State.Dead`) is still handled in `UAshAttributeSet::PostGameplayEffectExecute`.
- Reuses existing types (`UAshAbilitySystemComponent`, `UAshAttributeSet`, `EAshTeamId`)
  rather than introducing parallel systems.
- Deliberately a dev/verification helper, not the Phase 5 `AAshEnemyGeneralCharacter`:
  no AIController, Behavior Tree, abilities, or animation. This respects the "do not work
  on tasks outside the active phase" rule while still completing Phase 4's verification.
- One class per file, `Ash` prefix, data-tunable values (ARCHITECTURE.md 17, 18.1, 18.2).

## Testing / Verification

This is a new UCLASS, so Live Coding cannot register it — a real module rebuild is
required:

1. Close the Unreal Editor.
2. Rebuild the `AshDraftEditor` target (from the IDE, or:
   `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" AshDraftEditor Win64 Development -Project="C:\Users\Foryoucom\Documents\GitHub\AshDraft\AshDraft.uproject" -WaitMutex`).
3. Reopen the editor and the PoC map.
4. Drag an `AshGASTargetDummy` (or a Blueprint child with a visible mesh) into the level
   a couple of meters in front of the hero start.
5. PIE, walk up to the dummy, and trigger the basic attack (`IA_AttackBasic`).
6. Confirm the on-screen "took damage -> Health X / Y" message appears and the value
   drops each hit; with `InitialMaxHealth 100`, hero `AttackPower 25`, dummy
   `Defense 0`, expect ~25 per hit until it reaches 0 and turns red.

Optional: check the Output Log for `LogAshGASVerify` lines for a non-visual record.

## Result (confirmed 2026-06-21)

Verified in PIE. A `B_Dummy` Blueprint child of `AAshGASTargetDummy` placed in
`BaseLevel` was struck by the hero's basic attack; `LogAshGASVerify` recorded:
`100 → 75 → 50 → 25 → 0` (−25.0 per hit = AttackPower 25 − Defense 0), reaching zero
and applying `Ash.State.Dead`. The Phase 4 damage pipeline is validated end-to-end.

## Known Issues

- The dummy ships with no mesh; add one on a Blueprint child if a visible body is wanted.
- It does not respawn or reset Health — re-place or restart PIE to test again.

## Next Steps

- Run the PIE verification above and, once a Health drop is observed, check the final
  Phase 4 box and close the phase.
- Phase 5: `AAshEnemyGeneralCharacter` + AIController/Behavior Tree. The dummy can be
  retired (or kept as a test-only actor) once the real enemy general can take damage.
