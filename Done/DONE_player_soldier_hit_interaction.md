# DONE: Player ↔ Mass Soldier Hit Interaction (query-only hit capsule)

## Summary

Made Mass soldiers damageable by the hero (and any GAS melee attacker, e.g. the enemy general) by
giving the representation proxy a **query-only hit capsule** and bridging the resulting actor-space
hit back into the soldier's Mass health. Mass stays authoritative; no per-soldier physics body and
no GAS attribute set are introduced.

This is the AAA-correct hybrid: only the bounded set of promoted proxies (the soldiers near the
player) carry a hit volume, it is query-only (never blocks movement, never simulates physics, never
pushes neighbours), and far soldiers — which a melee hero cannot reach — carry nothing. Physics
collision is deliberately NOT used; it is the wrong tool at army scale and was the source of the
Phase 20 crowd shaking.

## Files Changed

- `Public/Mass/AshSoldierVisualConfig.h` — added `CapsuleRadius` / `CapsuleHalfHeight` (per-unit-type
  hit-capsule sizing).
- `Public/Mass/AshSoldierProxyActor.h` + `Private/Mass/AshSoldierProxyActor.cpp` — added the
  query-only `UCapsuleComponent HitCapsule`, a `GetRepresentedTeam()` accessor, and
  `ReceiveMeleeHit(Damage, Instigator)`. The capsule overlaps the Pawn channel only, is enabled on
  assignment / disabled when pooled, and is sized from the visual config in `ConfigureVisual`.
- `Private/AbilitySystem/Abilities/AshGA_BasicAttack.cpp` — `PerformHitSweep` now branches on
  `AAshSoldierProxyActor` hits: team-checked, deduped, and damaged by the attacker's `AttackPower`.
- `Docs/PLAN.md` — Phase 21 entry.

## Implementation Details

- **Hit detection.** The hero attack already does `SweepMultiByChannel(ECC_Pawn, Sphere(AttackRadius))`.
  The proxy capsule is `CollisionEnabled = QueryOnly`, object type `ECC_Pawn`, all responses `Ignore`
  except `ECC_Pawn = Overlap`. A multi-sweep returns overlapping components as touches, so the sweep
  finds the capsule; because the response is Overlap (not Block) the capsule never blocks the hero's
  movement and the player can move through the crowd. Collision is toggled on in `AssignToEntity` and
  off in `ClearAssignment`, so parked/pooled proxies are not stray targets.
- **Damage bridge.** Soldiers have no ASC, so the GE pipeline can't run on them. `PerformHitSweep`
  reads the attacker's `AttackPower` attribute (the same source magnitude `UAshDamageExecution`
  consumes) and passes it to `ReceiveMeleeHit`, which subtracts it from the entity's
  `FAshHealthFragment` via the Mass entity manager and sets `FAshCombatEventFragment::bWasHitThisTick`
  so the representation processor plays the flinch montage. The Mass death processor removes the
  soldier on zero health, exactly like a soldier-vs-soldier kill.
- **No friendly fire.** The attacker's team (`UAshTeamStatics::GetActorTeam`) is compared against the
  proxy's `RepresentedTeam`; only hostile soldiers are damaged. This also makes the enemy general's
  identical attack able to damage the player's soldiers, symmetrically.
- **Data-driven.** Capsule size is per unit type on the visual config; the capsule itself is a real
  component on `B_Soldier_Proxy`, so it is visible and tunable in the Blueprint editor (replacing the
  old hardcoded "mesh = NoCollision, nothing else" state).

## Architecture Notes

- Hybrid representation (ARCHITECTURE.md 13): the proxy is the near, high-fidelity body; the hit
  capsule is just another fidelity feature it carries, bounded by the proxy cap.
- Mass remains authoritative (ARCHITECTURE.md 7.4): the capsule is a hit *target*, the health write
  goes through the entity manager, and the existing combat/death processors do the rest.
- Soldier-vs-soldier and gravity stay non-physics (steering separation + ground trace, Phase 20); the
  capsule is Overlap-only specifically so it cannot re-introduce push/jitter.

## Testing / Verification

- Code review complete. **Build PENDING** — header changes added a UPROPERTY component, so this needs
  a full rebuild (and likely an editor restart, not just Live Coding):
  `"<UE_5.8>\Engine\Build\BatchFiles\Build.bat" LyraEditor Win64 Development -Project="<...>\AshDraft.uproject" -WaitMutex`.
- PIE (user): the hero's basic attack kills nearby **enemy** soldiers (health drops, flinch plays,
  they die) but does **not** damage **allied** soldiers. Confirm the player is not blocked/stuck when
  walking into a crowd (capsule is overlap-only). Tune `CapsuleRadius`/`CapsuleHalfHeight` on the
  `DA_*_Visual` assets if the hit feel is off.

## Known Issues

- Only promoted (LOD-0 / visible) soldiers have a capsule, so only they are hittable. That matches a
  melee hero's reach; ranged/AoE that must hit far soldiers should query the Mass grid, not physics.
- Damage uses the attacker's raw `AttackPower` minus an implicit 0 soldier-defense (clamped to 1).
  If soldiers later need a defense stat, add it to `UAshMassSoldierConfig` and subtract it here.
- The health write happens on the game thread outside the Mass processing phase (same pattern the
  combat processor uses for cross-entity writes); fine for the single-threaded PoC.

## Next Steps

- Optional: knockback / hit-stun on struck soldiers (impulse into the movement fragment).
- Optional: route soldier deaths-by-hero into the telemetry combat log (Phase 17) for QA bots.
