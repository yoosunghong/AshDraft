# Phase 20: Mass Soldier Local AI (Behavior State Machine) + Combat / Collision Polish

Goal: give every Mass soldier a correct, data-driven **local behavior state machine** and fix
the three field-visible defects (ground burial, wrong attack facing, crowd shaking / the larger
army shoving the smaller). AAA-grade, data-oriented, no per-soldier Actor / Tick / brain
(ARCHITECTURE.md 7.4 / 8.3 / 18.3; CLAUDE.md "soldiers execute simple local rules").

## Design decision: state *machine*, not a per-entity Unreal StateTree

PROPOSAL.md §15.2 reserves the Unreal **StateTree** for the *enemy general* (적 장수 — few agents,
complex decisions). §15.3 / §15.5 specify the *general soldier* (일반 병사) as a **Mass Entity with
squad membership, movement, formation-keeping, simple attack, and "병사 로컬 AI" (soldier local AI)**.

At army scale (hundreds–thousands of soldiers) the AAA-correct realization of that "state tree of
behavior" is a **data-oriented finite state machine evaluated by a Mass Processor over packed
fragments**, exactly how Epic's MassCrowd / MassAI drive large populations. A per-entity Unreal
StateTree asset would add a heavy instanced behavior context per soldier for a 3-state local rule —
the wrong tool here. The Unreal StateTree stays the tool for the General/Squad layer.

So the soldier "State tree" in this phase = an explicit `EAshSoldierState` machine
(`UAshMassSoldierBehaviorProcessor`), with every threshold authored in a data asset.

## Per-soldier behavior specification

### Hierarchy of authority (why local-only is correct)

```
Commander (UAshCommanderSubsystem)  ->  picks each squad's objective base
        v
Squad (UAshSquadSubsystem)          ->  publishes one shared objective location per squad
        v
Soldier (this phase)                ->  FOLLOWS the squad objective (general's command) and only
                                        does LOCAL actions: notice an enemy a few metres away,
                                        step into reach, strike. No map-wide target search.
```

Because the commander/squad already drive *where the army goes*, the soldier must **not** run a
macroscopic target search. It only senses enemies inside a small `LocalSenseRadius`, and it will
not chase one past `MaxLeashFromObjective` from its squad's objective — so soldiers never peel off
and wander the map; they hold the line their general assigned.

### States — `EAshSoldierState`

| State | Meaning | Movement | Facing | Combat |
|---|---|---|---|---|
| `FollowSquad` | No local enemy (or leashed out). Default. | Steer to squad objective via flow field (existing) | Face velocity | none |
| `Engage` | Local enemy sensed, not yet in reach | Steer directly toward the target, stop just inside `AttackRange` | Face the **target** | none |
| `Attack` | Local enemy within `AttackRange` | **Hold position (anchored)** | Face the **target** | Strike on cooldown |

Transition rules, evaluated each soldier's **AI-LOD tick** (time-sliced, Phase 12):

- Acquire nearest hostile within `LocalSenseRadius` (coarse spatial hash, O(n)).
- Reject it if its distance from the squad objective exceeds `MaxLeashFromObjective` (0 = no leash).
- Found & in `AttackRange` -> `Attack`; found & farther -> `Engage`; none -> `FollowSquad`.
- Target is written to `FAshCombatFragment::Target`; the combat processor only applies damage, it
  no longer searches (macroscopic search removed).

### Data-driven tuning — `UAshSoldierBehaviorConfig` (new data asset, one per unit type)

All numbers below are **data, not code** (CLAUDE.md: no hardcoded gameplay values). Referenced from
`UAshMassSoldierConfig::Behavior`, stamped onto each entity via `FAshBehaviorFragment`, and read by
the behavior / movement / ground processors. Processors keep sane fallbacks when it is null.

Sensing & leash
- `LocalSenseRadius` (cm, default 600) — how close an enemy must be before the soldier engages.
- `MaxLeashFromObjective` (cm, default 1200, 0 = unlimited) — how far from the squad objective a
  soldier may chase before it gives up and returns to following.

Facing (fixes "attacks while looking the wrong way")
- `TurnRateDegPerSec` (default 720) — how fast the body yaws toward its desired heading. Facing is
  **target-aware**: in `Engage`/`Attack` it faces the target, otherwise it faces travel — and it
  *interpolates* so turns read naturally instead of snapping.

Separation / anti-shaking (fixes jitter + the bigger army pushing the smaller)
- `SeparationRadius` (cm, default 90) — personal space; same-team neighbours inside this push apart.
- `SeparationStrength` (default 0.6) — push magnitude relative to MoveSpeed.
- `SeparationRelaxation` (default 0.5) — fraction of the computed push applied per frame; <1
  converges instead of oscillating (kills the shake).
- `MaxPushPerNeighbor` (default 1.0) — clamps a single overlapping neighbour's contribution so two
  stacked soldiers can't fling each other.
- `CombatAnchorResistance` (0..1, default 0.85) — while `Attack`, received push is scaled by
  `(1 - this)`, so an attacking soldier holds its ground. **Same-team only** separation + anchoring
  is what stops a denser formation from bulldozing the thinner enemy line.

Ground conform (fixes "half buried in the ground")
- `bConformToGround` (default true) — sample terrain height under the soldier and clamp its Z.
- `GroundTraceChannel` (default WorldStatic), `GroundTraceUp` (cm, 200), `GroundTraceDown` (cm, 1000).

> The per-mesh **art** offset (`UAshSoldierVisualConfig::MeshRelativeLocation.Z`, the pelvis->feet
> correction) stays where it is — it is measured per skeletal mesh in the editor. Ground conform
> fixes the *world* Z (entities used to be frozen at the spawner's flat height with no terrain
> follow); the mesh offset fixes the *local* art alignment. Both are needed; both are data.

## Processor pipeline (PrePhysics order)

```
SquadTracking -> Behavior(new) -> Movement(edited) -> Ground(new) -> Combat(edited) -> Representation(edited)
```

- `UAshMassSoldierBehaviorProcessor` (new): the state machine. Builds the hostile grid, time-sliced
  per-soldier acquisition + transitions, writes `FAshSoldierStateFragment` + `FAshCombatFragment::Target`.
- `UAshMassMovementProcessor` (edited): destination steering (unchanged) + **target-aware
  interpolated facing** (`FacingYaw`) + **same-team, relaxed, clamped, combat-anchored** separation;
  reads per-unit tunables from `FAshBehaviorFragment`.
- `UAshMassGroundProcessor` (new): for visible (LOD<=`MaxConformLOD`) soldiers, line-traces down and
  clamps `Movement.Position.Z` onto the ground. Game thread.
- `UAshMassCombatProcessor` (edited): **damage-only** — resolves the behavior-set target, applies
  damage in range on cooldown, raises attack/hit events. No more map-wide acquisition or LOD gate
  (the behavior processor owns the AI time-slice).
- `AAshSoldierProxyActor` (edited): faces from the Mass-authoritative `FacingYaw` instead of raw
  velocity (so a stopped, attacking soldier still faces its enemy).

## Tasks

- [x] `EAshSoldierState` + `FAshSoldierStateFragment` + `FAshBehaviorFragment` (+ trivially-copyable
      trait opt-out) added to `AshSoldierFragments.h`; `FacingYaw` added to `FAshMovementFragment`
- [x] `UAshSoldierBehaviorConfig` data asset (sensing / leash / facing / separation / ground)
- [x] `UAshMassSoldierConfig` gains a `Behavior` reference (unit type -> behavior set)
- [x] `UAshMassSoldierBehaviorProcessor` — local-only state machine (Follow/Engage/Attack)
- [x] `UAshMassMovementProcessor` — interpolated target-aware facing; same-team relaxed/clamped/
      anchored separation; per-unit tunables
- [x] `UAshMassGroundProcessor` — terrain Z conform for visible soldiers
- [x] `UAshMassCombatProcessor` — reduced to damage application driven by the behavior target
- [x] `AAshSoldierProxyActor::SyncFromEntity` — facing from `FacingYaw`
- [x] `AAshMassSoldierSpawner` — archetype + seeding of the new fragments / behavior config
- [x] `Done/DONE_mass_soldier_local_ai.md`
- [ ] BUILD (blocked): editor open / Live Coding active when attempted — rebuild with editor closed
      or Ctrl+Alt+F11 (see guide §0)
- [x] EDITOR: authored `DA_Soldier_Behavior` (shared) and linked it from `DA_MassSoldierConfig_Ally`
      and `DA_MassSoldierConfig_Enemy.Behavior` (created + saved live via the editor bridge).
- [ ] EDITOR (user): set the per-mesh `MeshRelativeLocation.Z` on each `DA_*_Visual`; PIE-verify
      burial, facing, and no-shake in a 2-army fight. Guide:
      `Docs/Guides/Phase20_MassSoldier_Behavior_Setup_Guide.md`. (No State Tree asset required — the
      soldier "state tree" is the C++ FSM in `UAshMassSoldierBehaviorProcessor`.)

## Phase 20.1 addendum: engage-on-contact + distributed deployment

Two further field reports after the first Phase 20 pass:

### 1. Combat didn't start until soldiers were near the destination

The leash that keeps soldiers "holding the line" was measured as **distance from the squad's final
objective**. While an army marches toward a *distant* objective through the flow field, every soldier
is far beyond `MaxLeashFromObjective`, so the behavior processor rejected every sensed enemy and no
target was ever acquired — combat only began once the line had advanced to within the leash of the
goal. That contradicts the intent: a soldier should switch to *eliminate the enemy* the moment one
enters its sight, then rejoin the advance.

**Fix.** The leash is now measured from the soldier's **engage anchor** — the position where it first
made contact — instead of the objective:

- Acquire the nearest hostile within `LocalSenseRadius` and engage it **immediately** (no objective
  gate). Combat triggers on contact during the march.
- On first contact, stamp `FAshSoldierStateFragment::EngageAnchor = current position`, `bEngaged = true`.
- Keep pursuing only while within `MaxLeashFromObjective` of that anchor; past it, drop the target and
  return to `FollowSquad` (the flow field carries the soldier back to the line). The anchor is kept
  while leashed-out so the soldier doesn't immediately re-chase the same straggler from its new spot.
- No hostile in range → clear target, `bEngaged = false`, `FollowSquad`.

`MaxLeashFromObjective` keeps its name (so the authored `DA_Soldier_Behavior` value is preserved) but
now means *max chase distance from the engage anchor*. The squad-objective lookup (and the squad
subsystem dependency) is removed from the behavior processor.

### 2. Clustered soldiers shook; the centre got stuck

Soldiers steered to the **exact** objective point at **full `MoveSpeed`** until within
`ArrivalTolerance`. More than ~4 cannot occupy one point, so separation shoved the surplus out and
they re-accelerated straight back at full speed — a per-frame ram-and-rebound that jitters the whole
cluster and traps the centre. Velocity-relaxation alone couldn't damp it because the *steering* side
kept ramming at full speed.

**Fix (movement processor):**

- **Arrival ease-in** — within `ArrivalSlowdownRadius` of a group objective the steer speed scales
  with the remaining distance (`Speed *= clamp(dist / radius, 0, 1)`), so soldiers settle onto the
  goal instead of ramming and rebounding.
- **Distributed deployment** — during the existing same-team separation sweep, if an advancing
  soldier finds a squad-mate both *ahead* (toward the goal) and *nearer the goal* than itself, it is
  treated as having reached the formation edge: it drops its forward press and lets separation seat it
  at spacing behind that rank. Rear ranks fan out into a stable, minimum-distance formation around the
  objective rather than piling onto the one point. Combat-target chasing and `Attack` anchoring are
  untouched (the rule is gated to group-objective advance).

New data-driven tunable `UAshSoldierBehaviorConfig::ArrivalSlowdownRadius` (default 250 cm), with the
same processor fallback pattern as the other separation knobs.

## Tasks (20.1)

- [x] `FAshSoldierStateFragment` gains `EngageAnchor` + `bEngaged`
- [x] `UAshSoldierBehaviorConfig`: `MaxLeashFromObjective` re-documented as anchor-relative chase
      leash; new `ArrivalSlowdownRadius`
- [x] `UAshMassSoldierBehaviorProcessor` — engage-on-contact, anchor-relative leash; squad-objective
      leash + squad subsystem dependency removed
- [x] `UAshMassMovementProcessor` — arrival ease-in + distributed-deployment stop; new fallback
      `ArrivalSlowdownRadius`
- [x] `Done/DONE_mass_soldier_engage_and_deploy.md`
- [ ] BUILD (user): editor is open — new `UPROPERTY`s change struct reflection, so rebuild with the
      editor **closed** (a full build, not Live Coding).
- [ ] EDITOR/PIE (user): PIE-verify a soldier engages an enemy crossing its path mid-march and rejoins
      the flow field after; verify a dense cluster settles with no centre shake.
