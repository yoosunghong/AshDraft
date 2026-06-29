---
# DONE: Battlefield Combat Feel — Stable Deployment + Melee Dissolve (Phase 26)

## Summary

Review-driven rework of the large-scale clash so it reads like a real battlefield. Two field reports
were fixed at the root:

1. **Squads no longer march back and forth** to reach the matched enemy squad. The duel ring is now
   *persistent* across replans (stable orientation axis + a fixed angular slot per fireteam + a smoothed
   centre), so a committed squad keeps its seat and marches there once instead of being handed a freshly
   rotated ring every half second. The engaged formation anchor sits *at* the contact (not 420 cm
   behind), so a soldier that loses its target no longer snaps backward (the squad "breathing").

2. **The front line dissolves into a jumbled individual melee** with rear engagement. On contact the
   fireteam stops dictating squad-vs-squad slot targets and hands soldier-vs-soldier targeting to a
   capped, distinct-pairing acquisition: each soldier takes a *different* nearby enemy, and the surplus
   reaches the nearest *open* (often deeper) enemy — organic flanking + rear engagement. An opt-in weak
   enemy anti-stack lets opposing lines interpenetrate without bodies overlapping, so the clean wall
   becomes a swirl.

Plus the structural cleanup the review called out: a shared matching helper removes the duplicated
algorithm, the fireteam god-method is split, and hardcoded constants moved to data.

## Files Changed

New:
- `Public/AI/AshEngagementMatcher.h` / `Private/AI/AshEngagementMatcher.cpp` — the dependency-free
  `AshEngagement` module: `BalancedPairing` (greedy 1:1 + capped surplus doubling + hysteresis) and
  `AcquireCappedNearest` (nearest under-cap hostile with keep-stickiness; mutates a shared claim map).

Modified:
- `Public/AI/AshBattleSubsystem.h` / `Private/AI/AshBattleSubsystem.cpp` — persistent `FAshBattleRingState`
  per battle (keyed by sorted general-id hash): smoothed `CentreEMA`, fixed `AxisDir`, persistent
  `SlotIndexOfFireteam`, growing `SlotCount`, `PrevAssignment` for hysteresis; matching delegated to
  `AshEngagement::BalancedPairing`; adaptive replan timer (`EnsureReplanTimer`, reads `ReplanPeriod`).
- `Public/AI/AshGeneralConfig.h` — `RingRecenterSmoothing`, `ReplanPeriod`.
- `Public/Mass/AshMassFireteamProcessor.h` / `.cpp` — `Execute` split into decompose / stamp-battle /
  `MatchmakeFallback` (shared matcher + actor fallback) / apply; melee hand-off on Engaged (publishes the
  contact anchor, clears only the actor assignment, leaves `Combat.Target` to the behavior processor);
  de-hardcoded `FireteamsPerRow` + `FireteamStandbyDistance`; `LastFallbackAssignment` hysteresis state.
- `Public/Mass/AshSoldierFragments.h` — `FAshFormationFragment::ContactAnchor` / `bHasContactAnchor`.
- `Public/Mass/AshSoldierBehaviorConfig.h` — `MaxAttackersPerEnemySoldier`, `MeleeChaseRadius`,
  `MeleeTargetStickiness`, `EnemySeparationRadius`, `EnemySeparationStrength`, `FireteamSlotOffsets`.
- `Private/Mass/AshMassSoldierBehaviorProcessor.cpp` — sense grid `GatherHostiles` + a claim-count tally;
  `AcquireCappedNearest` acquisition; contact-anchored melee leash; removed the per-slot MassEntity branch.
- `Private/Mass/AshMassMovementProcessor.cpp` / `.h` — target-loss hold; opt-in weak enemy anti-stack in
  the separation sweep; de-hardcoded `FormationStopDistance`.
- `Private/Mass/AshMassSoldierSpawnLibrary.cpp` — reads the data-driven `FireteamSlotOffsets` (built-in V fallback).
- `Docs/PLAN.md`, `Docs/Plan/Phase26.md`.

## Implementation Details

**Shared matcher (kills the duplication).** `AshEngagement::BalancedPairing` is the one implementation of
"greedy 1:1 by mutual proximity, capped surplus doubling, with a stickiness bonus for last plan's
partner". The battle director uses it to lay out duels on the ring; the fireteam processor's
out-of-battle fallback uses it for proximity skirmishing. `AcquireCappedNearest` is the per-soldier
analogue: the nearest hostile whose attacker count is under a cap (the current target may be kept even
at cap, with a distance discount), mutating a shared claim map so a batch of soldiers spreads across
distinct enemies. Both are pure (no Mass/World), so they are unit-testable.

**Stable ring (Issue #1 A).** The subsystem now persists ring state between replans. The centre is a
low-pass filter of the generals' mean; the orientation axis is captured once at battle formation; each
fireteam keeps a fixed angular slot index for the battle's life (a wiped duel leaves a gap, filled by
the next new duel, and `SlotCount` only grows — so survivors never shift angle). The replan timer adapts
to the generals' authored `ReplanPeriod`.

**No backward snap (Issue #1 B/C).** The engaged anchor is the contact point itself (the ring slot, or
the midpoint out of battle), not `contact + ownSide*420`, so there is no side-flip and a target-less
soldier drifts to the contact rather than 420 cm backward. The movement processor additionally *holds*
an Engage/Attack soldier with no resolved target instead of falling back to the squad/base objective.

**Melee dissolve (Issue #2).** On Engaged the fireteam publishes `ContactAnchor` and clears only the
actor assignment; `Combat.Target` is owned by the behavior processor, which acquires via
`AcquireCappedNearest` against the local sense grid and a frame claim map. Distinct pairing + the
attacker cap push surplus soldiers to the nearest *open* enemy (deeper ranks) → flanking / rear
engagement with no special case. The melee leash is anchored to `ContactAnchor` (`MeleeChaseRadius`) so
the brawl stays a local bubble but soldiers may cross the original line. The movement processor's
separation sweep gained an opt-in weak *enemy* anti-stack (`EnemySeparationRadius`, default 0) so
opposing bodies slip past instead of forming a wall — the jumble — while same-team spacing, the
distributed-deployment stop, and the Z anti-climb guard are unchanged.

## Architecture Notes

- **No Actor Tick** (CLAUDE.md 18.3): director is timer-driven; execution stays in the Mass processors.
- **God-object reduced**: the fireteam `Execute` is split into focused passes; the matching algorithm
  lives once in `AshEngagement` (no duplication between subsystem and processor).
- **Data-driven** (ARCHITECTURE.md 17): every new gameplay number is a `UPROPERTY` on a config; the
  hardcoded V offsets / column count / stop thresholds were moved to data with built-in fallbacks.
- **No new hard dependencies**: soldiers still carry ids/handles, never object pointers across layers;
  the helper module references no engine subsystems.
- **Non-destructive defaults**: `EnemySeparationRadius = 0` reproduces the prior hard standoff, so the
  jumble is opt-in per unit type; with no battle active the proven fallback path runs unchanged.

## Testing / Verification

- BUILD: `LyraEditor Win64 Development` — **Succeeded** (2026-06-29, editor closed). Clean compile + link
  of `AshDraftCoreRuntime`; no new warnings (only pre-existing GAS deprecation + plugin-dependency notes).
- PENDING USER (PIE): restart the editor (struct/UPROPERTY changes). Place an Ally and an Enemy
  `AAshGeneralCharacter` with troops. `Ash.Battle.Debug 1`. Expect: when the generals close, both armies
  split into a ring of duels and **march once** to their seats (no back-and-forth); on contact the lines
  **jumble** into individual fights; surplus soldiers reach **rear** enemies; no vertical climbing. Tune
  `DA_*_Behavior` (`EnemySeparationRadius` to dial the jumble, `MaxAttackersPerEnemySoldier`,
  `MeleeChaseRadius`) and `DA_General_*` (`DuelRingRadius`, `RingRecenterSmoothing`, `ReplanPeriod`).

## Known Issues

- **Two-sided battles** (inherited): a battle splits into side A (first fireteam's team) vs everything
  hostile; a true 3-faction free-for-all needs a richer split.
- **Battle re-key on roster change**: keying a battle by its sorted general-id set means a general
  joining/leaving re-keys it (ring re-forms once). Fine for the 1v1-general PoC.
- **Readability vs chaos**: high `EnemySeparationRadius` + low `MaxAttackersPerEnemySoldier` can read as
  mush; these are the dials to trade order ↔ chaos per unit type.
- **Parallelism / scale**: the combat processors remain game-thread (`bRequiresGameThreadExecution`) and
  rebuild per-frame spatial structures; the army-scale parallelism path is deferred to a perf phase.
- PIE verification itself is PENDING USER (editor work).

## Next Steps

- Author the data: enable the jumble on `DA_*_Behavior`; tune the ring on `DA_General_*`; PIE-verify.
- Optional: a battle-wide morale/rout state (when a side collapses, the loser breaks and the ring
  dissolves into a pursuit) for an even more dramatic swing.
- Optional perf phase: parallelize the Mass combat processors + share one spatial hash across them.
- Optional: unit tests for `AshEngagement::BalancedPairing` / `AcquireCappedNearest` (pure functions).
---
