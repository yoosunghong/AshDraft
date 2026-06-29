---
# DONE: Unit Death Animations (soldiers + generals, corpse removed after 5 s)

## Summary

Dead units now play a death animation and linger as a corpse for a configurable window (default 5 s)
before being removed, instead of vanishing the instant their health hits zero.

- **Mass soldiers:** the death processor begins a *corpse window* on death rather than reaping the entity
  immediately; the representation proxy plays the unit's death montage and holds the body until the window
  elapses, then the entity is reaped.
- **Generals (`AAshGeneralCharacter`):** play a death montage in `HandleDeath`; the body already despawns
  after `DeathLifeSpan` (5 s).
- **Hero (`AAshHeroCharacter`):** plays a death montage in `HandleDeath` (the player pawn is not removed —
  death is terminal defeat owned by the match flow). This fills the long-standing placeholder there.

All timings/assets are data-driven (CLAUDE.md: no hardcoded gameplay values).

## Files Changed

- `Public/Mass/AshSoldierFragments.h` — new `FAshDeathFragment` (`bIsDying`, `DeathTime`, `bDeathAnimStarted`).
- `Public/Mass/AshSoldierVisualConfig.h` — new `DeathMontage` on the unit visual set.
- `Public/Mass/AshSoldierProxyActor.h` / `Private/Mass/AshSoldierProxyActor.cpp` — `PlayDeath()` plays the
  death animation in **single-node mode** (`MeshComponent->PlayAnimation(DeathAnim, false)`) which holds the
  final frame, and disables the hit capsule; `AssignToEntity` restores `AnimationBlueprint` mode when a
  pooled corpse is recycled for a live soldier.
- `Public/Mass/AshMassDeathProcessor.h` / `Private/Mass/AshMassDeathProcessor.cpp` — corpse window:
  `DeathDisplayDuration` (config, default 5 s); mark dying on first zero-health frame, reap after the window.
- `Private/Mass/AshMassRepresentationProcessor.cpp` — keep a dying soldier's existing proxy through the
  window and call `PlayDeath()` once; ordered `ExecuteAfter AshMassDeathProcessor`; query gains
  `FAshDeathFragment`.
- `Private/Mass/AshMassSoldierSpawnLibrary.cpp` — added `FAshDeathFragment` to the soldier archetype.
- `Public/Character/AshGeneralCharacter.h` / `Private/Character/AshGeneralCharacter.cpp` — `DeathAnim`
  UPROPERTY, played single-node in `HandleDeath`.
- `Public/Character/AshHeroCharacter.h` / `Private/Character/AshHeroCharacter.cpp` — `DeathAnim`
  UPROPERTY, played single-node in `HandleDeath`.

## Implementation Details

- **Inert corpse.** A dying soldier keeps `CurrentHealth == 0`. Every gameplay processor (movement, combat,
  behavior, fireteam, sensing) already early-outs on `CurrentHealth > 0`, and the behavior sense grid marks
  dead entries `bAlive = false`, so a corpse neither moves, fights, nor is targeted — it only animates. No
  new "is dead" checks were needed across the combat stack; the existing health gate does the work.
- **Death processor (delayed reap).** On the first frame at zero health it sets `bIsDying`/`DeathTime`
  (synchronous, not deferred) and does *not* destroy. Once `Now - DeathTime >= DeathDisplayDuration` it
  defers `DestroyEntity` as before. World time comes from `Context.GetWorld()->GetTimeSeconds()`.
- **Representation ordering.** The representation processor now runs `ExecuteAfter AshMassDeathProcessor`,
  so on the kill frame it already sees `bIsDying` and routes the entity to the corpse path instead of
  demoting/releasing its proxy.
- **Holding the death pose (single-node, no AnimBP authoring).** A montage blends back to the AnimBP's idle
  when it ends (the "returns to standing" bug). Instead the proxy plays the death **AnimSequence** in
  single-node mode (`USkeletalMeshComponent::PlayAnimation(anim, bLooping=false)`): this bypasses the AnimBP
  and **freezes on the final frame** for the whole corpse window — guaranteed, with no IsDead state to
  author. `AssignToEntity` restores `AnimationBlueprint` mode (re-creating the locomotion instance from the
  still-applied AnimClass) when a pooled corpse is recycled, so a fresh soldier animates normally again.
- **Pool priority.** A corpse only *keeps* a proxy it already had (`FindProxy`); it never `AcquireProxy`s a
  new one. This prevents a wave of corpses from starving living soldiers of the bounded proxy pool in a
  large battle. Corpses that leave LOD 0 demote normally and free their proxy.
- **Characters.** `GetMesh()->PlayAnimation(DeathAnim, false)` likewise holds the final frame (single-node),
  a harmless no-op if no anim/mesh is set. The general's existing `SetLifeSpan(DeathLifeSpan)` handles the
  5 s removal; the hero is intentionally not removed.

## Architecture Notes

- Data-oriented (ARCHITECTURE.md 7): death state is a compact POD fragment processed in batches; the
  death/representation split is preserved (death owns the lifecycle, representation owns the view).
- No Actor Tick (18.3): everything is processor-, animation- or `SetLifeSpan`-driven.
- Data-driven tuning (17): the corpse window and every death animation are assets/UPROPERTYs, not constants.
- `FAshDeathFragment` is trivially copyable (two bools + a float), so no `TMassFragmentTraits` opt-out is
  needed (unlike the `TObjectPtr`-bearing fragments).

## Testing / Verification

- Build: LyraEditor Win64 Development — **Succeeded** (2026-06-29, editor closed). Only the two pre-existing
  harmless warnings (GAS `ActivationInfo` deprecation; EnhancedInput/GameplayStateTree plugin-dependency).
- PENDING USER (editor + PIE):
  1. Assign `DeathAnim` (a death **AnimSequence**, not a montage) on each `DA_<Unit>_Visual`, and on the
     general/hero Blueprints. No AnimBP authoring is required — single-node playback holds the final frame.
  2. PIE: kill soldiers near the player — confirm the death animation plays, **holds the downed pose** (no
     return to standing), and the body despawns ~5 s later; kill a general — confirm it animates, holds, then
     despawns; confirm no live soldier ever shows a downed pose (proxy reuse).

## Known Issues

- A corpse that dies while *not* already proxied (off-screen / capped out at the moment of death) shows no
  death animation; it is still reaped after the window. This is by design (living soldiers keep pool priority).
- Single-node death bypasses the AnimBP, so additive/blended death effects authored in the AnimBP won't play
  on a soldier corpse. (The clip itself, including its final pose, plays fully.)
- `DeathDisplayDuration` is a single processor-wide value; per-unit-type corpse windows could move onto
  `UAshSoldierVisualConfig` later if needed.

## Next Steps

- Optional: ragdoll/physics-blend on death for the high-fidelity Characters (general/hero).
- Optional: fade-out / sink the soldier corpse near the end of the window instead of a hard despawn.
