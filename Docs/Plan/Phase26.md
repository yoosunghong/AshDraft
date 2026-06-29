# Phase 26: Battlefield Combat Feel — Stable Deployment + Melee Dissolve

Goal: make the large-scale clash *feel like a real battlefield* — squads still **deploy deliberately
to multiple spread contact points** (no more squads marching back and forth to reach a foe), but on
contact the rigid squad-vs-squad line **dissolves into an individual, jumbled melee** where soldiers
pair off with distinct opponents, interpenetrate, and reach the enemy's rear ranks. AAA-grade,
data-oriented, no per-soldier Actor / Tick / brain (ARCHITECTURE.md 7.4 / 8.3 / 18.3).

This is a **review-driven** phase. It addresses two field reports and the structural debt found while
reviewing the Phase 20→24 combat stack:

1. **Squads move back and forth instead of committing** to the matched enemy squad.
2. **Soldiers fight too clustered and the front line is unnaturally regular** — they only ever attack
   the enemy directly ahead; no flanking, no rear engagement, no "chaos of contact."

Chosen direction (confirmed with the user): **Hybrid** — keep the deliberate, spread deployment
(stabilised), then dissolve to individual melee on contact. Plus the structural cleanup the review
called out (extract the shared matching helper, split the fireteam god-method, de-hardcode constants).

---

## 1. Root-cause summary (what the review found)

### Issue #1 — back-and-forth squad movement

| # | Root cause | Where |
|---|---|---|
| A | The duel ring is recomputed from scratch every 0.5 s replan: slot angles are derived from *live* duel midpoints (`BaseAngle`) and `2π/NumDuels`, so a committed fireteam is handed a **different ring slot** every replan and re-marches. A deploying fireteam chases a slot that teleports and may never "arrive". **Dominant cause.** | `AshBattleSubsystem.cpp` `Replan()` ring block (sort by live midpoint → `BaseAngle` from live midpoint → even-angle slots) |
| B | The engaged formation anchor is `Target + normalize(MyAvg − Target) * CombatAnchorDistance`. As a squad presses past the contact point, `OwnSide` **flips sign** and the anchor jumps to the far side. | `AshMassFireteamProcessor.cpp` battle + non-battle engaged anchor |
| C | On per-soldier target loss (matched enemy died), movement falls back to the **420-back formation anchor** and the soldier walks backward, then re-acquires and walks forward — the squad "breathes". | `AshMassMovementProcessor.cpp` destination priority (formation fallback) |
| D | Out-of-battle fireteam matchmaking is **rebuilt every frame, greedy, no hysteresis** → the nearest-enemy choice flips frame-to-frame when 3+ squads are near. | `AshMassFireteamProcessor.cpp` candidate/greedy block |

### Issue #2 — clustered + regular front line

| # | Root cause | Where |
|---|---|---|
| E | Engagement is modeled at **squad granularity**: every member of fireteam A targets the *matched slot* soldier of fireteam B. Squads meet as blocks at discrete points → clusters. | `AshMassFireteamProcessor.cpp` `SlotEntities.Find(SlotIndex)` |
| F | The rigid **V formation is held into contact**; nothing dissolves it on impact. | `AshMassFireteamProcessor.cpp` `DesiredWorldPosition = Anchor + V`; `AshMassSoldierSpawnLibrary.cpp` hardcoded V |
| G | **Interpenetration is forbidden** — stop at `AttackRange*0.85` and hold, same-team-only separation, attacker anchor resistance. Correct Phase 20-23 anti-climb/anti-shove fixes, but together they make a clean standoff line. | `AshMassMovementProcessor.cpp` combat approach + separation |
| H | A soldier only ever sees the **single nearest hostile** within `LocalSenseRadius`, and the leash + flow-field return herd strays back into line. No path to a rear-rank opponent. | `AshMassSoldierBehaviorProcessor.cpp` `FindNearestHostile` + leash |

### Structural debt

- `UAshMassFireteamProcessor::Execute` is a ~370-line **god method** (decompose + match + gang + actor
  fallback + publish + per-entity apply).
- The **greedy 1:1-by-proximity match is duplicated** in `AshBattleSubsystem` and the fireteam
  processor (two copies that must stay in sync; they already differ).
- **Three overlapping movement-target systems** (squad objective / fireteam anchor / per-soldier
  target) with precedence encoded ad-hoc across two processors.
- **Hardcoded gameplay numbers** persist (V offsets, `%3` columns, `150.f`/`60.f` thresholds,
  standoff fallbacks, `ReplanPeriod`), against the data-driven rule (ARCHITECTURE.md 17 / CLAUDE.md).

---

## 2. Design decisions

### D1. Split of responsibility: *where* clashes vs *how* they dissolve

```
Battle director (UAshBattleSubsystem)  ->  WHERE squads clash: a STABLE, spread set of contact points
        v                                  (the duel ring, persistent across replans)
Fireteam processor (UAshMassFireteamProcessor) -> march each squad to its committed contact point
        v                                  (Deploying), then mark Engaged and HAND OFF to the melee
Soldier behavior (UAshMassSoldierBehaviorProcessor + FAshMeleeSolver) -> HOW it dissolves locally:
                                           distinct individual pairing, surplus seeks the nearest
                                           OPEN enemy (incl. rear ranks), leashed to the contact point
```

The squad layer keeps the **deliberate spread** (the part that already works, once stabilised). The
soldier layer gains the **emergent jumble**. This preserves the existing hierarchy (ARCHITECTURE.md 8)
and the data-oriented model — the behavior processor already owns per-soldier target selection and the
AI-LOD time-slice, so the dissolve is an *upgrade to its acquisition rule*, not a new system.

### D2. One shared matching helper (the "common algorithm helper class" from the review)

A new dependency-free module **`AshEngagement`** (no Mass / no World types → unit-testable) is the single
home for the engagement math, removing the duplication:

- `AshEngagement::BalancedPairing(...)` — greedy 1:1 by mutual proximity + capped surplus doubling +
  **hysteresis**. Used by the battle director (fireteam↔fireteam) *and* the fireteam processor's
  out-of-battle fallback. One algorithm, two callers.
- `AshEngagement::AcquireCappedNearest(...)` — nearest hostile whose attacker-count is under a cap,
  with target persistence. The melee-dissolve primitive (soldier↔soldier). Same "capped greedy" idea
  as the squad matcher, applied per soldier — which is exactly why it belongs in the same module.

```cpp
namespace AshEngagement
{
    struct FMatchUnit { int32 Id = INDEX_NONE; FVector Pos = FVector::ZeroVector; int32 Side = 0; };

    struct FPairingParams
    {
        float MaxPairDist     = 0.f;   // 0 = unlimited
        int32 MaxAttackers    = 2;     // surplus cap (1v1 -> 1v2, never 1v5)
        float StickinessBonus = 0.f;   // effective-distance discount for the previous partner (hysteresis)
    };

    // Side A vs Side B (Side field), distance-2D. PrevAssignment carries last frame's id->id for hysteresis.
    void BalancedPairing(TArrayView<const FMatchUnit> Units, const FPairingParams& Params,
                         const TMap<int32,int32>& PrevAssignment, TMap<int32,int32>& OutAssignment);

    // Returns the chosen enemy id (INDEX_NONE if none). ClaimCounts is mutated (the choice is claimed).
    // PreferKeep = the soldier's current target id; kept if still valid & under cap & in range.
    template<typename FNeighbourQuery>
    int32 AcquireCappedNearest(int32 SelfId, const FVector& From, float MaxRange, int32 MaxAttackers,
                               int32 PreferKeep, TMap<int32,int32>& ClaimCounts, FNeighbourQuery&& Query);
}
```

### D3. Keep StateTree on generals; the melee stays a Mass data-oriented pass

No per-entity StateTree for soldiers (PROPOSAL.md 15.2/15.3/15.5 unchanged). The dissolve is a
claim-map + capped-greedy pass over packed fragments — the AAA-correct realization at army scale.

---

## 3. Issue #1 — stop the oscillation (correctness)

### 3.1 Stable duel ring (fixes A)

The battle subsystem keeps **persistent ring state per battle** between replans (it currently keeps
none — every `Replan()` starts from scratch):

```cpp
struct FAshBattleRingState
{
    FVector  CentreEMA   = FVector::ZeroVector; // low-pass filtered battle centre (smooth drift)
    FVector  AxisDir     = FVector::ForwardVector; // stable orientation = general-vs-general axis at formation
    TMap<int32,int32> SlotIndexOfFireteam;       // fireteamId -> fixed angular index, persistent
    int32    NextFreeSlot = 0;
};
TMap<uint32, FAshBattleRingState> RingStates; // keyed by a stable battle key (sorted general-id set hash)
```

Replan rules:
- **Battle key** = hash of the sorted participating general ids (stable while the same generals fight;
  documented limitation: a general joining/leaving re-keys that battle — acceptable, rare).
- **Centre**: `CentreEMA = Lerp(CentreEMA, meanGeneralPos, RingRecenterSmoothing)` (e.g. 0.25) — drifts
  smoothly instead of snapping.
- **Axis**: captured once at battle formation from the two generals' positions; the ring's angle-0 is
  this axis, so the orientation never rotates with live midpoints.
- **Slot index**: each fireteam keeps its `SlotIndexOfFireteam` entry for the battle's life. A new
  fireteam gets `NextFreeSlot++`. A wiped fireteam's index is **retired but not reused mid-battle**
  (gaps are fine — the ring just has an empty seat) so surviving fireteams never shift angle.
- **Slot position**: `Centre + Rotate(AxisDir, angleOf(slotIndex, slotCount)) * RingRadius`. Same
  fireteam → same angle every replan → the squad marches once and stays.

> Net: the ring becomes a *fixed set of seats* a squad is assigned to and holds, rather than a fresh
> circle every half-second. This alone removes the "deploy, back, deploy" churn.

### 3.2 Non-flipping own-side anchor (fixes B)

Derive a fireteam's "side of the contact" from its **team's general → battle-centre axis** (a team is
always on its general's side; this never flips during a melee), not from the live `MyAvg − Target`:

```
OwnSide = normalize(MyGeneralPos - Centre)   // stable for the whole battle
Anchor  = Slot + OwnSide * CombatAnchorDistance
```

For the non-battle fallback (no general/centre), cache the approach direction at first engagement and
apply a small **deadband** before allowing it to flip.

### 3.3 Target-loss hold (fixes C)

In the movement processor, an `Engage`/`Attack` soldier that *momentarily* has no resolved target
(its enemy died this frame) **holds** (velocity damped to ~0) until the behavior processor's next
AI-LOD tick reassigns one — it must **not** fall back to the far formation anchor. With the melee
solver the reassignment is a nearby enemy, so the soldier turns to a new opponent instead of retreating.

### 3.4 Matchmaking hysteresis (fixes D)

Route the out-of-battle fallback through `AshEngagement::BalancedPairing` with `StickinessBonus > 0`,
so a fireteam keeps its current enemy unless a new one is closer by more than the margin.

---

## 4. Issue #2 — melee dissolve (the battlefield feel)

When a fireteam reaches its (now stable) contact point it flips to **Engaged** and **hands off** to the
melee layer instead of dictating per-slot targets.

### 4.1 Fireteam processor change

- On `Engaged`, **stop** setting each member's `Combat.Target` from `EnemyFireteam->SlotEntities`
  (delete the slot-matched targeting, root cause E). Set `FireteamState = Engaged`, publish the
  **contact anchor** (the stable slot) into a fragment the behavior/movement layer reads, and let the
  soldiers acquire individually.
- `Deploying` is unchanged (march straight to the slot, local sensing suppressed) — this is the
  deliberate spread we keep.

### 4.2 Behavior processor change — `FAshMeleeSolver`

For an `Engaged` (or locally-contacting) soldier, replace single-nearest acquisition with capped,
distinct pairing:

```
Pass 0 (O(n)):  build ClaimCounts[enemyId] from every soldier's CURRENT target (stable; only
                time-sliced soldiers re-decide this frame).
Per time-sliced soldier:
   keep current enemy if still alive & within MeleeChaseRadius of the ContactAnchor & under cap;
   else enemy = AshEngagement::AcquireCappedNearest(self, pos, LocalSenseRadius,
                MaxAttackersPerEnemySoldier, currentEnemy, ClaimCounts, senseGridQuery)
   leash = MeleeChaseRadius measured from ContactAnchor (NOT the map objective)
```

Consequences (the desired chaos):
- **Distinct pairing** — soldiers fan out to *different* enemies instead of stacking the matched slot.
- **Surplus seeks the nearest OPEN enemy** — once front enemies hit the attacker cap, extra soldiers
  pick the nearest *under-cap* enemy, which is deeper → **organic flanking + rear engagement**, no
  special-case code (it falls out of "nearest under-cap").
- **Local brawl, not map-wide** — the `MeleeChaseRadius`-from-contact leash keeps the jumble a bubble
  around each contact point; the battle stays spread across the ring's points but each point churns.

### 4.3 Movement processor change — allow the jumble

- **Drop formation cohesion in melee** — already the case once a soldier has an individual target
  (combat approach overrides the formation desired-position); with 4.1 there is no per-slot pull left.
- **Weak opposing anti-stack** — add a *small* enemy-separation term, active only at very short range
  and much weaker/narrower than same-team separation, so opposing bodies don't perfectly overlap but
  **can slip past each other** (lines blend/jumble). `EnemySeparationRadius = 0` reproduces today's
  hard standoff, so this is opt-in data.
- **Keep the guards** — Z anti-climb stays (Phase 25 ground multi-trace already skips pawns + Pawn-typed
  capsules, so XY interpenetration cannot cause vertical climbing), and the whole-army anti-bulldoze
  (same-team relaxed/clamped push) stays.

Result: squads arrive at spread points (deliberate), then each point becomes a swirling individual
melee with interpenetration and rear engagement — the `ox ox / ox / ox ox` scatter, not a clean line.

---

## 5. Processor pipeline (unchanged order; responsibilities shift)

```
SquadTracking -> Fireteam(edited) -> Behavior(edited, +FAshMeleeSolver) -> Movement(edited) -> Ground -> Combat -> Representation
Battle director (UAshBattleSubsystem): timer-driven, off-frame; now stateful (persistent ring).
```

- `UAshBattleSubsystem` — persistent ring state; `BalancedPairing`; smooth centre/axis. (D2, 3.1)
- `UAshMassFireteamProcessor` — **split** into `DecomposeFireteams()` / `Matchmake()` (→ `BalancedPairing`) /
  `ApplyFireteamState()`; stop per-slot melee targeting; stable own-side; publish contact anchor. (god-method fix, E, B, D)
- `UAshMassSoldierBehaviorProcessor` — `FAshMeleeSolver` acquisition for engaged soldiers; contact-anchored leash. (H, 4.2)
- `UAshMassMovementProcessor` — target-loss hold; optional weak enemy anti-stack; formation yields to target. (C, G, 4.3)
- `AshEngagement` (new module) — `BalancedPairing` + `AcquireCappedNearest`. (D2)

---

## 6. Data-driven tunables (no hardcoding — ARCHITECTURE.md 17 / CLAUDE.md)

New on `UAshSoldierBehaviorConfig` (per unit type):
- `MaxAttackersPerEnemySoldier` (default 2) — melee claim cap (distinct-pairing pressure).
- `MeleeChaseRadius` (cm, default ~700) — local-brawl leash from the contact point.
- `MeleeTargetStickiness` (cm, default ~150) — keep-current margin.
- `EnemySeparationRadius` (cm, default 0 = today's hard standoff) + `EnemySeparationStrength` — opt-in
  opposing anti-stack that lets lines interpenetrate.

New on `UAshGeneralConfig` (Battle section):
- `RingRecenterSmoothing` (0..1, default 0.25) — battle-centre EMA factor.
- `ReplanPeriod` (s, default 0.5) — promote the constexpr to data.

New `AshEngagement::FPairingParams` fed from the above (`StickinessBonus`, `MaxAttackers`, `MaxPairDist`).

De-hardcode (move constants to data):
- The 5-soldier V offsets → a `FAshFireteamFormation` struct (on `UAshSoldierBehaviorConfig` or a small
  shared formation asset); the spawn library and fireteam re-form read the **same** source.
- The `%3` column count → `FireteamsPerRow` (config).
- `150.f` standby / `60.f` formation stop / standoff `110.f`/`0.85f` fallbacks → named config fields.

---

## 7. Files

New:
- `Public/AI/AshEngagementMatcher.h` / `Private/AI/AshEngagementMatcher.cpp` — `AshEngagement` module
  (`FMatchUnit`, `FPairingParams`, `BalancedPairing`, `AcquireCappedNearest`). Pure; no Mass/World deps.
- (optional) `Public/Mass/AshFireteamFormation.h` — `FAshFireteamFormation` (data-driven V).

Modified:
- `Private/AI/AshBattleSubsystem.cpp` + `.h` — persistent `RingStates`; `BalancedPairing`; smoothing.
- `Public/AI/AshGeneralConfig.h` — `RingRecenterSmoothing`, `ReplanPeriod`.
- `Public/Mass/AshMassFireteamProcessor.h` + `.cpp` — split methods; remove slot-matched melee
  targeting; stable own-side; contact-anchor publish; `BalancedPairing` fallback.
- `Public/Mass/AshMassSoldierBehaviorProcessor.h` + `.cpp` — `FAshMeleeSolver` acquisition.
- `Private/Mass/AshMassMovementProcessor.cpp` + `.h` — target-loss hold; enemy anti-stack fallbacks.
- `Public/Mass/AshSoldierBehaviorConfig.h` — melee tunables; formation data.
- `Public/Mass/AshSoldierFragments.h` — `FAshSoldierStateFragment::ContactAnchor` (struct change →
  editor-closed build); `FAshFormationFragment` may carry the contact anchor instead.
- `Private/Mass/AshMassSoldierSpawnLibrary.cpp` — read the data-driven V.
- `Docs/PLAN.md` — Phase 26 entry.

---

## 8. Tasks

Design:
- [x] Review the Phase 20→24 combat stack; root-cause the back-and-forth + clustered/regular line.
- [x] This spec (`Docs/Plan/Phase26.md`) + PLAN.md entry. **(awaiting user approval before code)**

Implementation:
- [x] `AshEngagement` module — `BalancedPairing` (greedy 1:1 + capped doubling + hysteresis),
      `AcquireCappedNearest`. (`Public/AI/AshEngagementMatcher.h` + `Private/AI/AshEngagementMatcher.cpp`.)
- [x] `UAshBattleSubsystem` — persistent ring state (stable axis + persistent slot index + smoothed
      centre); route matching through `BalancedPairing`; adaptive replan timer. (Issue #1 A)
- [x] `UAshMassFireteamProcessor` — split into decompose / stamp battle / `MatchmakeFallback` (shared
      matcher) / apply; contact-anchor publish; melee hand-off (no per-slot target); de-hardcoded
      column count + standby distance. (Issue #1 B/D, Issue #2 E/F)
- [x] `UAshMassSoldierBehaviorProcessor` — capped distinct acquisition via `AcquireCappedNearest`
      (claim map), contact-anchored melee leash. (Issue #2 H, rear engagement)
- [x] `UAshMassMovementProcessor` — target-loss hold; opt-in weak enemy anti-stack; de-hardcoded
      formation stop distance. (Issue #1 C, Issue #2 G)
- [x] Data: melee tunables on `UAshSoldierBehaviorConfig` (`MaxAttackersPerEnemySoldier`,
      `MeleeChaseRadius`, `MeleeTargetStickiness`, `EnemySeparation*`, `FireteamSlotOffsets`); ring
      tunables on `UAshGeneralConfig` (`RingRecenterSmoothing`, `ReplanPeriod`); contact anchor on
      `FAshFormationFragment`; de-hardcoded the spawn V.
- [x] BUILD — LyraEditor Win64 Development **Succeeded** (2026-06-29, editor closed). Clean compile +
      link of `AshDraftCoreRuntime`; no new warnings.
- [x] `Done/DONE_battlefield_combat_feel.md`.
- [ ] EDITOR/PIE (user): restart the editor (struct/UPROPERTY changes); optionally set melee tunables on
      `DA_*_Behavior` (raise `EnemySeparationRadius` to enable the jumble; tune `MaxAttackersPerEnemySoldier`,
      `MeleeChaseRadius`) and ring tunables on `DA_General_*`; `Ash.Battle.Debug 1`. Verify: squads deploy
      to spread points and **hold** (no back-and-forth), lines **jumble** on contact, surplus soldiers
      reach **rear** enemies, no vertical climbing.

---

## 9. Risks & open questions

- **Reviving "climbing"** by allowing XY interpenetration: mitigated — Phase 25 ground conform already
  ignores pawns and Pawn-typed capsules, so overlapping bodies don't climb. `EnemySeparationRadius`
  defaults to 0 (today's hard standoff) so the change is opt-in and tunable.
- **Multi-battle ring keying**: keying by the sorted general-id set re-keys a battle if a general
  joins/leaves. Fine for the 1v1-general PoC; a stable allocated battle id is the follow-up if N-general
  battles become common.
- **Readability vs chaos**: full interpenetration can read as mush. `MaxAttackersPerEnemySoldier`,
  `EnemySeparationRadius`, and `MeleeChaseRadius` are the dials to trade order ↔ chaos per unit type.
- **Performance**: the melee claim map is O(n) per frame (same budget as the existing sense grid);
  `BalancedPairing` stays O(F²) in fireteam count (small). The army-scale parallelism path
  (`bRequiresGameThreadExecution`, shared spatial hash) is **out of scope** here — noted for a later
  performance phase.
