# DONE: Mass Soldier Engage-on-Contact + Distributed Deployment (Phase 20.1)

## Summary

Two field-reported defects in the Mass soldier local AI were fixed, both data-driven:

1. **Combat now triggers on contact during the march.** Previously a soldier marching toward a
   distant flow-field objective would not engage enemies it met en route — combat only began once the
   line had advanced near the destination. The objective-relative engagement leash was the cause; it
   is now measured from where the soldier first made contact, so a soldier switches to *eliminate the
   enemy* the instant one enters its sense radius, then rejoins the advance once it is dead.

2. **Clustered soldiers no longer shake / trap the centre.** Soldiers rammed the exact objective point
   at full speed and the surplus was shoved out and re-accelerated back every frame. Movement now eases
   speed in near a group objective and stops a soldier pressing in once a squad-mate already fills the
   space ahead, so ranks fan out into a stable, minimum-distance formation.

## Files Changed

- `Public/Mass/AshSoldierFragments.h` — `FAshSoldierStateFragment` gains `EngageAnchor` (FVector) and
  `bEngaged` (bool) for the anchor-relative chase leash.
- `Public/Mass/AshSoldierBehaviorConfig.h` — `MaxLeashFromObjective` re-documented as the max chase
  distance **from the engage anchor** (name kept so the authored `DA_Soldier_Behavior` value is
  preserved); new `ArrivalSlowdownRadius` (default 250 cm).
- `Public/Mass/AshMassSoldierBehaviorProcessor.h` — header doc updated (engage-on-contact, anchor leash).
- `Private/Mass/AshMassSoldierBehaviorProcessor.cpp` — engage-on-contact + anchor-relative leash;
  removed the squad-objective leash, the `FAshSquadFragment` query requirement, the `UAshSquadSubsystem`
  dependency and its include.
- `Public/Mass/AshMassMovementProcessor.h` — new fallback `ArrivalSlowdownRadius`.
- `Private/Mass/AshMassMovementProcessor.cpp` — arrival ease-in for group objectives + distributed
  deployment "blocked-ahead" stop folded into the existing separation sweep.
- Docs: `Docs/PLAN.md` (Phase 20.1 entry), `Docs/Plan/Phase20.md` (20.1 addendum + tasks).

## Implementation Details

### Engage-on-contact + anchor-relative leash (`AshMassSoldierBehaviorProcessor`)

The state machine no longer gates acquisition on distance to the squad objective. Each AI-LOD tick:

- Acquire the nearest hostile in `LocalSenseRadius` (unchanged coarse spatial hash). If none → clear
  target, `bEngaged = false`, `FollowSquad`.
- On first contact (`!bEngaged`): `EngageAnchor = SelfPos`, `bEngaged = true`.
- Keep engaging only while `Dist2D(EngageAnchor, SelfPos) <= MaxLeashFromObjective` (0 = unlimited);
  in range → `Attack`, else `Engage`. Past the leash → drop target, `FollowSquad`, but **keep the
  anchor** so the soldier doesn't immediately re-chase the same straggler from its new position.

Because sensing is local and small, "engage on contact" cannot turn into a map-wide chase: the anchor
leash bounds pursuit to a few metres of where the soldier stood when it first met the enemy. The
movement processor already chases `FAshCombatFragment::Target` directly and returns to the flow field
when the target is cleared, so "return to the flow field after the enemy is eliminated" is automatic.

### Arrival ease-in + distributed deployment (`AshMassMovementProcessor`)

Steering was split into a pre-separation `SteerVelocity` so the neighbour sweep can veto the forward
press in the same pass:

- **Ease-in:** when advancing on a *group* objective, `Speed = MoveSpeed * clamp(dist/ArrivalSlowdownRadius, 0, 1)`.
  Near the goal the steering force shrinks to match the separation force, so the crowd settles instead
  of ramming-and-rebounding (the dominant shake cause). Combat-target chasing keeps full speed.
- **Blocked-ahead:** the existing same-team separation sweep also flags `bBlockedAhead` when a
  neighbour within `SeparationRadius` is both ahead (`dot(N - self, AdvanceDir) > 0`) and nearer the
  destination (`Dist2D(N, Dest) < Dist2D(self, Dest)`). A blocked soldier zeroes its steer velocity and
  lets separation seat it at spacing behind that rank. The frontmost soldier (no nearer neighbour)
  still reaches the goal, so ranks fill outward in spaced layers — the "distributed deployment".

Separation, anchoring, clamping and target-aware facing are otherwise unchanged.

## Architecture Notes

- Still no per-soldier Actor/Tick/brain: both fixes are data-oriented edits to existing Mass
  processors over packed fragments (ARCHITECTURE.md 7.4 / 18.3; CLAUDE.md "soldiers execute simple
  local rules"). Soldiers remain local-only — the commander/squad layer still owns *where* the army
  goes; this only changes *how a soldier reacts to what crosses its own path*.
- All new behaviour is tunable as data (`UAshSoldierBehaviorConfig`), with processor fallbacks when
  the config is null (CLAUDE.md: no hardcoded gameplay values).
- The behavior processor shed a dependency (squad subsystem) rather than adding one.

## Testing / Verification

Code review only — **not yet built or PIE-verified** (the editor was open; new `UPROPERTY`s on the
Mass fragment/struct change reflection and are not Live-Coding-safe). To verify:

1. Close the editor and rebuild (Win64 Development Editor — a full build, not Live Coding).
2. PIE: send an army to a far objective with an enemy between it and the goal. The soldiers should
   engage the enemy on contact mid-march and resume the flow-field advance after killing it.
3. PIE: mass a dense cluster on one objective. It should ease in and settle into a spaced formation
   with no centre jitter, and idle soldiers should hold a minimum distance.

## Known Issues

- `MaxLeashFromObjective` keeps its now-slightly-misleading name to preserve the authored asset value;
  the doc comment explains it is anchor-relative. Rename + re-author if a cleaner name is wanted.
- The blocked-ahead test uses `SeparationRadius` as the block distance, so the formation packs to
  shoulder spacing before fanning out; if a looser formation is wanted, add a separate block radius.
- While leashed-out next to a stationary enemy whose objective sits on top of the soldier, it will
  hold without fighting (by design — "hold the line, don't chase"); rare in practice.

## Next Steps

- User: rebuild (editor closed) and run the two PIE checks above; tune `ArrivalSlowdownRadius` and
  `MaxLeashFromObjective` per unit type on `DA_Soldier_Behavior` if needed.
- Optional: promote the "blocked-ahead" formation into a proper formation-slot assignment
  (`UAshMassFormationProcessor`) if designers want authored shapes rather than emergent packing.
