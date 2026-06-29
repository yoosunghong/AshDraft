---
# DONE: Target Attack-Slot / Surround System (Phase 28)

## Summary

Applied external Dynasty-Warriors / Musou combat advice whose two core asks — the **Target Slotting
System** ("reserve a slot *around* the enemy, not the enemy itself; limit slots per target") and the
**cinematic waiting** tip ("only 3–5 soldiers actually attack while the rest hold a combat-idle ring
and circle") — were the genuine gaps in an otherwise mature combat stack (the fireteam duel ring and
count-capped melee dissolve already existed from Phases 24/26).

Both gaps are closed with one mechanism: a per-target **attack-slot ring** at the individual soldier ↔
individual target granularity. When several soldiers commit to one target (a Mass enemy soldier **or**
an actor general/hero), only the first `ActiveAttackerCount` take **inner** slots and strike; the rest
take an **outer** ring — a new `Surround` state where they hold a wider radius, slowly orbit, face the
target and menace, but never strike. A circler is promoted to an inner slot the instant one frees, so
the crowd rotates.

The design is **ratio-adaptive**, which is why it improves feel without regressing the existing army
battles: with a modest per-target cap a balanced clash still pairs off 1v1/1v2 (no surround); where
soldiers outnumber targets (around the hero, a lone general, the last survivor) they form a real
surround ring — the iconic Musou shot.

## Files Changed

- `Public/Mass/AshSoldierFragments.h` — new `EAshSoldierState::Surround`; new
  `FAshSoldierStateFragment::SlotAngle` (the orbit bearing). **Fragment layout change.**
- `Public/Mass/AshSoldierBehaviorConfig.h` — new `ActiveAttackerCount`, `SurroundRingGap`,
  `SurroundOrbitSpeed`; `MaxAttackersPerEnemySoldier` reframed as the full ring capacity (strikers +
  waiters) and default bumped 2→4.
- `Private/Mass/AshMassSoldierBehaviorProcessor.cpp` — pass A tallies inner-ring holders per target
  (Mass + actor); pass B assigns each committed soldier an inner (strike) or outer (`Surround`) slot,
  seeding the orbit bearing on entry. Applies to both the Mass-target dissolve and the actor-target path.
- `Private/Mass/AshMassMovementProcessor.cpp` — `Surround` soldiers steer to an orbiting outer-ring slot
  (state fragment promoted to ReadWrite for the per-frame angle drift); target-loss hold + target-facing
  now include `Surround`.
- `Private/Mass/AshMassCombatProcessor.cpp` — a `Surround` soldier keeps its cooldown ready but never
  strikes (reads the state fragment).
- `Private/Mass/AshMassRepresentationProcessor.cpp`, `Private/Mass/AshSoldierProxyActor.cpp`,
  `Public/Mass/AshSoldierProxyActor.h`, `Public/Mass/AshSoldierAnimInstance.h` — optional combat-stance
  hook: `bInCombatStance` is pushed to the proxy/AnimBP for engaging/striking/surrounding soldiers.
- `Docs/PLAN.md` — Phase 28 entry.

## Implementation Details

- **Where it lives.** The slot decision is layered into the existing melee dissolve, not a new system.
  `AshEngagement::AcquireCappedNearest` still picks *which* enemy a soldier commits to (capped, distinct,
  rear-engaging surplus); Phase 28 only decides *how* the committers arrange around that enemy.
- **Inner vs outer.** Two small tally maps built in pass A (`InnerUsed` keyed by Mass entity index,
  `ActorInnerUsed` keyed by actor unique id) count current strikers per target. In pass B a soldier keeps
  an inner seat if it already held one on the same target (stability), else claims a free inner seat if
  `InnerUsed[target] < ActiveAttackerCount`, else surrounds. This mirrors the existing `ClaimCounts`
  pattern exactly, so it is allocation-light and self-healing across frames.
- **Slot geometry.** The slot bearing is the soldier's own `atan2(self − target)` at the moment it enters
  the ring — so it holds its own side (no cross-running) and the ring fills naturally from the approach
  directions. Outer-ring soldiers drift the bearing by `SurroundOrbitSpeed * dt` each frame in the
  movement processor; the outer radius is `max(strikerStandoff + SurroundRingGap, AttackRange * 1.15)`,
  always past strike range so waiters physically ring the target. The soldier tracks its moving slot with
  an overshoot-capped step, which reads as a smooth circle rather than a queue. Same-team separation seats
  the lateral spacing within each ring.
- **Backward compatibility.** Existing `DA_*_Behavior` assets serialize `MaxAttackersPerEnemySoldier = 2`;
  with `ActiveAttackerCount` defaulting to 3, only 2 commit and both are inner → identical to pre-Phase-28
  behavior. Surround only activates when a designer raises the cap above the active count, or for soldiers
  with no behavior config (new code fallback: cap 4, active 3).

## Architecture Notes

- Pure data-oriented Mass processing: no per-soldier Actor/Tick/StateTree, no new fragment *type* (one new
  field on an existing fragment), no new subsystem. Behavior stays in processors; tunables stay on the
  `UAshSoldierBehaviorConfig` data asset (CLAUDE.md: no hardcoded gameplay values, ARCHITECTURE.md 7.4).
- Reinforces the hierarchy (ARCHITECTURE.md 8/10): the Battle director still owns the fireteam duel ring;
  this is the soldier-local rule that distributes individuals around a target — the data-oriented analogue
  of the actor-only `UAshCombatSlotComponent` (10.1 "limit the number of active attackers / readable
  surround"), now available to Mass soldiers.
- Mass stays authoritative; the proxy/anim change is a cosmetic read-only hook.

## Testing / Verification

- Build: **pending** — rebuild LyraEditor Win64 Development with the **editor closed** (new fragment field
  + enum value change reflection; not Live-Coding-safe).
- PIE (pending user):
  1. Raise `DA_<Unit>_Behavior.MaxAttackersPerEnemySoldier` to ~6–8 (above `ActiveAttackerCount`, default 3).
  2. Let an enemy soldier mass converge on the hero or a lone general → a few (≈3) strike at the inner
     standoff while the rest circle the target and menace, not piling on its exact point.
  3. Confirm a balanced 30v30 army clash still pairs off (no mass surround / no piling) — no regression.
  4. Optionally branch the minion AnimBP idle on `bInCombatStance` to show a guard pose while circling.

## Known Issues

- **Balance shift (intended):** a surrounded hero/general now takes hits from only `ActiveAttackerCount`
  soldiers at a time instead of everyone in range — more survivable (the Musou wade-through). Tune
  `ActiveAttackerCount` up for more incoming pressure.
- Slot bearing comes from approach direction + separation, not a globally packed angular assignment, so
  distribution is even but not perfectly equi-angular; acceptable at PoC scale and cheaper.
- Advice items deliberately **not** taken this phase (lower leverage given existing systems): Reynolds
  alignment/cohesion weights, and explicit avoidance of the dueling generals' space (the guard-fireteam +
  duel-ring + separation already keep that area readable). Candidates for a follow-up if needed.

## Next Steps

- Optional: a `Ash.Combat.SlotDebug` CVar to draw each target's inner/outer ring + occupancy.
- Optional: concentric multi-ring (3+ rings) for very large surrounds, and per-ring orbit direction.
- Optional follow-up: Reynolds alignment/cohesion + general-space avoidance if the melee still reads busy.
