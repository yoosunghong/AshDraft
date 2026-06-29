# DONE: Player-in-the-Crowd Combat Fixes

## Summary

Fixes three field defects reported when the hero fights inside the Mass soldier crowd:

1. **Player gets trapped** when surrounded by enemy generals/soldiers — unable to move or rotate.
2. **Soldiers stack vertically** ("step on each other and rise") when densely packed.
3. **Enemy soldiers ignore the player**, wandering instead of attacking, even when surrounding the hero.

All three are code-only; no new data assets.

## Files Changed

- `Source/AshDraftCoreRuntime/Private/Mass/AshSoldierProxyActor.cpp` — proxy hit capsule forced
  overlap-only at runtime (defect 1).
- `Source/AshDraftCoreRuntime/Private/Mass/AshMassGroundProcessor.cpp` — ground conform multi-trace that
  skips bodies (defect 2).
- `Source/AshDraftCoreRuntime/Private/Mass/AshMassFireteamProcessor.cpp` — actor-target fallback now also
  considers the hostile player pawn (defect 3).
- `Source/AshDraftCoreRuntime/Public/Character/AshHeroCharacter.h` /
  `Source/AshDraftCoreRuntime/Private/Character/AshHeroCharacter.cpp` — new `ReceiveSoldierDamage` entry
  point (defect 3).
- `Source/AshDraftCoreRuntime/Private/Mass/AshMassCombatProcessor.cpp` — actor-damage branch now strikes a
  hero or a general (defect 3).
- `Docs/PLAN.md` — Phase 25 entry.

## Implementation Details

### 1. Player trapped when surrounded
The soldier proxy `HitCapsule` is, by design, a **query-only / overlap-only** volume (the melee sweep's
target from Phase 21). Crowd spacing is the data-oriented steering separation in the movement processor —
soldiers must **never physically block**, or a ring of them walls the player in (and with
`bOrientRotationToMovement`, a player who cannot move also cannot rotate). The C++ constructor set it up
correctly, but a blocking collision preset authored on `B_Soldier_Proxy` would override that. `AssignToEntity`
now **re-asserts overlap-only collision at runtime** every time a proxy is bound to an entity (object type
`ECC_Pawn`, ignore all channels, overlap `ECC_Pawn` only), so the capsule can never block movement
regardless of Blueprint authoring.

### 2. Soldiers climb / rise when packed
`UAshMassGroundProcessor` conforms a soldier's Z to the ground via a downward trace, but it snapped Z to the
**first blocking hit** of a single-line trace. A Character capsule (the hero, a general) blocks
`ECC_WorldStatic`, so a soldier overlapping a body traced onto that body's surface and "climbed" it. The
trace is now `LineTraceMultiByChannel`, walking the hits and taking the first that is **real terrain** —
skipping any `APawn` (hero/generals, capsule and mesh) and any `ECC_Pawn`-typed component (soldier proxy hit
capsules). Soldiers conform to the ground only.

**Follow-up:** unlike the original single-line trace, a *multi*-trace also returns non-blocking *overlap*
(touch) hits. The base's `CaptureVolume` is a query-only `OverlapAllDynamic` sphere, so soldiers began
climbing onto its overlap dome. The loop now also skips any hit with `!bBlockingHit`, conforming only to a
solid blocking surface.

### 3. Enemy soldiers ignore the player
Soldiers chose targets from exactly two sources, **neither of which could ever be the player**:
- the fireteam processor's actor-target fallback, which iterated only registered **generals**;
- the behavior processor's local sense grid, which contains only **Mass entities** (other soldiers).
The hero is a high-fidelity `ACharacter`, not a Mass entity, so it had no targeting path; squads with no
enemy soldiers/generals near the player fell through to "follow the squad objective" and milled about.

Fix mirrors the existing general path:
- **Fireteam processor**: the no-enemy-fireteam fallback now also considers the **hostile player pawn**
  (nearest of {generals, player} within `GeneralActorEngageRadius`, skipping a dead hero).
- **Combat processor**: the `Actor` target branch resolves a general *or* a hero and applies damage to
  whichever it is.
- **Hero**: new `ReceiveSoldierDamage(Amount, Instigator)` drives Health on the attribute set (mirrors
  `AAshGeneralCharacter::ReceiveSoldierDamage`); the existing health-changed delegate raises the defeat flow
  at zero.
The existing movement (steers to `CombatTarget.ActorTarget`) and behavior (Engage/Attack + target-aware
facing) actor-target paths already work generically, so no change was needed there.

## Architecture Notes

- Stays data-oriented: crowd spacing remains steering separation; no per-entity physics collision was added
  (that was explicitly rejected in Phase 20/21 as the cause of crowd shaking). The proxy capsule stays a
  query-only sweep target.
- No new Actor Tick; all logic lives in existing Mass processors and event-driven character entry points.
- Reuses the established team layer (`UAshTeamStatics`) and the general's damage pattern; the hero and
  general now share an identical soldier-damage seam.
- Tunables unchanged (`GeneralActorEngageRadius`, ground-trace fields, attack range), so designers retain
  control with no new assets to author.

## Testing / Verification

Build LyraEditor Win64 Development **with the editor closed** (header change: new `UFUNCTION` on
`AAshHeroCharacter` is not Live-Coding-safe). Then in PIE:
1. Walk the hero into a tight enemy crowd — confirm the hero can still move and turn in every direction
   (no trapping).
2. Pack soldiers densely (spawn a large army, let lines collide) — confirm soldiers stay on the ground and
   do not stack vertically or ride up onto the hero/generals.
3. Stand the hero among hostile soldiers with no friendly squad nearby — confirm they close in and attack
   (hero health drops), instead of wandering.

Not yet PIE-verified (editor was open during implementation; build deferred to the user).

## Known Issues

- A fireteam already paired with an enemy fireteam will fight that squad rather than the player; the player
  is targeted by squads that have **no enemy fireteam in range**. This is intentional (squads commit to
  their duel) and matches the "lone hero surrounded" complaint, but means the player is not always the
  priority target in a mixed melee.
- If `B_Soldier_Proxy` still has a blocking capsule preset, the runtime override now neutralizes it, but the
  Blueprint default is left as-is; consider also fixing the asset for clarity.

## Next Steps

- PIE-verify the three scenarios and tune `GeneralActorEngageRadius` for how eagerly soldiers peel off to
  the player.
- Optional: give individual soldiers (not just the fireteam) local sensing of the player so a soldier
  standing next to the hero engages immediately even mid-duel.
