# DONE: Mass Soldier Skeletal Meshes + Animations

## Summary

Rendered Mass soldiers are now real skeletal-mesh characters instead of debug points / static
meshes, and they animate in response to combat: a soldier plays its **attack** montage when its
strike lands and its **hit-react** montage when it takes damage. The visible body is the existing
Phase 13 LOD-0 proxy actor; combat events are surfaced from the Mass combat processor to that
proxy through a new one-shot event fragment.

## Files Changed

- `Public/Mass/AshSoldierFragments.h` — new `FAshCombatEventFragment` (one-shot
  `bAttackedThisTick` / `bWasHitThisTick` flags).
- `Public/Mass/AshSoldierProxyActor.h` / `Private/Mass/AshSoldierProxyActor.cpp` — body changed
  from `UStaticMeshComponent` to `USkeletalMeshComponent`; added `AttackMontage` /
  `HitReactMontage` (EditDefaultsOnly), `PlayAttackMontage()` / `PlayHitReactMontage()`, movement
  facing, and montage cleanup on recycle.
- `Private/Mass/AshMassCombatProcessor.cpp` — sets the attacker's attack flag and the victim's hit
  flag when a strike lands (victim flagged via the entity manager).
- `Private/Mass/AshMassRepresentationProcessor.cpp` — consumes the event flags (plays the proxy
  montages), passes velocity to the proxy for facing, runs after the combat processor, and clears
  the flags every frame.
- `Private/Mass/AshMassSoldierSpawner.cpp` — adds `FAshCombatEventFragment` to the soldier archetype.

## Implementation Details

- **Event surfacing is data-oriented.** Combat lives in the processor, not on the Actor; the proxy
  stays a brainless view (no AI, no Tick). The combat processor only writes two booleans; the
  representation processor (which already visits every soldier each frame, after movement) reads
  them, drives the montages on promoted proxies, and clears them — so each event animates exactly
  once and a far/off-screen or proxy-capped soldier never replays a stale event.
- **Ordering.** `UAshMassRepresentationProcessor` now declares `ExecuteAfter` both the movement and
  combat processors, so attack/hit montages play the same tick the strike happens.
- **Visuals are editor-data-driven.** The skeletal mesh, AnimBP, and the two montages are assigned
  on a Blueprint subclass of `AAshSoldierProxyActor` (the proxy class is set on the representation
  processor CDO / `DefaultGame.ini`), matching the existing "assign a mesh in a Blueprint subclass"
  pattern. Anim cost is bounded via `OnlyTickPoseWhenRendered`.

## Architecture Notes

- Follows ECS rules (CLAUDE.md / ARCHITECTURE.md 7): behavior in processors, data in fragments,
  no per-soldier brain or Actor Tick. Mass stays authoritative; the proxy mirrors it.
- Follows the Phase 13 hybrid-representation bound: only LOD-0 soldiers get a proxy, so the number
  of animating skeletal meshes is capped (`MaxActiveProxies`) regardless of army size.
- No new hard dependencies; the proxy never references the player or AI.

## Testing / Verification

- **Not yet compiled here** — the UE build is run via LyraEditor (Win64 Development) on the user's
  machine; this change is source-only and picked up automatically (module compiles all .cpp).
- PENDING USER (editor + PIE):
  1. Create/confirm a `BP_AshSoldierProxy` (subclass of `AAshSoldierProxyActor`); set its skeletal
     mesh + AnimBP and assign `AttackMontage` and `HitReactMontage`. Ensure the representation
     processor's `ProxyClass` points at it.
  2. PIE: bring soldiers into LOD 0 near the player and let two hostile groups engage; confirm
     skeletal soldiers render, face their movement, play the attack montage when striking and the
     hit-react montage when damaged.

## Known Issues

- Per-archetype visual variety is not wired through `UAshMassSoldierConfig` yet; all proxies of one
  proxy class share the same mesh/montages. A Mass shared fragment carrying the visual set would
  let different spawners use different bodies — a clean follow-up.
- No death animation: the death processor still removes entities outright. A "play death montage,
  then despawn" path is future work.
- Hit-react restarts on each hit (acceptable for the PoC); blending/queueing is a polish item.

## Next Steps

- Optionally move the visual set onto `UAshMassSoldierConfig` via a shared fragment for per-spawner
  bodies, and add a death montage with a short linger before despawn.
