# DONE: Mass Soldier Local AI (Behavior State Machine) + Combat / Collision Polish

## Summary

Gave every Mass soldier a data-driven **local behavior state machine** (the soldier "state tree")
and fixed the three field-visible defects: soldiers buried in the ground, soldiers attacking while
facing the wrong way, and crowd shaking / the larger army shoving the smaller while attacking.

Soldiers now act **locally only**: the commander/squad layer decides where the army goes, and each
soldier just follows that objective and engages enemies sensed within a small radius (no map-wide
target search). Every threshold is authored in a new `UAshSoldierBehaviorConfig` data asset, so
soldiers are tuned as data per unit type (CLAUDE.md: no hardcoded gameplay values).

This is Phase 20 (`Docs/Plan/Phase20.md`). Design decision: a data-oriented FSM over Mass fragments
is the AAA-correct realization of a soldier "state tree" at army scale; Unreal StateTree stays
reserved for the few complex General/Squad agents (PROPOSAL.md §15.2/15.3/15.5).

## Files Changed

Created
- `Public/Mass/AshSoldierBehaviorConfig.h` — per-unit-type behavior data asset (sensing / leash /
  facing / separation / ground conform).
- `Public/Mass/AshMassSoldierBehaviorProcessor.h` + `Private/Mass/AshMassSoldierBehaviorProcessor.cpp`
  — the local-AI state machine (FollowSquad / Engage / Attack).
- `Public/Mass/AshMassGroundProcessor.h` + `Private/Mass/AshMassGroundProcessor.cpp` — terrain Z
  conform for visible soldiers.
- `Docs/Plan/Phase20.md`, `Docs/Guides/Phase20_MassSoldier_Behavior_Setup_Guide.md`.

Modified
- `Public/Mass/AshSoldierFragments.h` — `EAshSoldierState`, `FAshSoldierStateFragment`,
  `FAshBehaviorFragment` (+ trivially-copyable trait opt-out), and `FacingYaw` on `FAshMovementFragment`.
- `Public/Mass/AshMassSoldierConfig.h` — added a `Behavior` reference (unit type -> behavior set).
- `Private/Mass/AshMassMovementProcessor.cpp` + `.h` — target-aware **interpolated facing**;
  **same-team, relaxed, clamped, combat-anchored** separation; per-unit tunables from the behavior
  config (processor keeps fallbacks).
- `Private/Mass/AshMassCombatProcessor.cpp` + `.h` — reduced to **damage application** driven by the
  behavior-selected target; removed map-wide acquisition + its own LOD gate.
- `Private/Mass/AshSoldierProxyActor.cpp` + `.h` — `SyncFromEntity` faces from the Mass-authoritative
  `FacingYaw` instead of raw velocity.
- `Private/Mass/AshMassRepresentationProcessor.cpp` — passes `FacingYaw` into `SyncFromEntity`.
- `Private/Mass/AshMassSoldierSpawner.cpp` — added the two new fragments to the archetype and seeds
  the behavior config / state.
- `Docs/PLAN.md` — Phase 20 entry.

## Implementation Details

Processor pipeline (PrePhysics):
`SquadTracking -> Behavior(new) -> Movement(edited) -> Ground(new) -> Combat(edited) -> Representation(edited)`.

- **Behavior processor** (the state machine): builds a coarse same-frame spatial hash of all
  soldiers (cell sized to the largest `LocalSenseRadius` so a 3×3 lookup always covers it), then —
  throttled per entity by the AI-LOD interval (it now owns the AI time-slice) — acquires the nearest
  hostile within `LocalSenseRadius`, rejects it if the soldier has strayed past
  `MaxLeashFromObjective` from its squad objective, writes the target into `FAshCombatFragment::Target`
  and sets `EAshSoldierState` to Attack (in range) / Engage (sensed, out of range) / FollowSquad (none).
- **Movement processor**: destination steering is unchanged (combat target → squad objective →
  nearest base, flow-field for group goals). New: an Attack-state soldier holds position; separation
  pushes only **same-team** crowders, each neighbour clamped (`MaxPushPerNeighbor`), the push relaxed
  (`SeparationRelaxation` < 1) so a cluster settles instead of oscillating, and scaled down for
  attackers (`CombatAnchorResistance`) so the front line holds. Facing is computed every frame
  (face the target while engaging/attacking, else face travel) and **interpolated** at the unit's
  `TurnRateDegPerSec` into `FacingYaw`.
- **Ground processor**: for visible soldiers (LOD ≤ `MaxConformLOD`), line-traces down on the config's
  channel and clamps `Position.Z` onto the hit, after movement integrates and before the proxy reads it.
- **Combat processor**: every frame advances the cooldown, resolves the behavior-selected target, and
  applies `AttackPower` when in range + off cooldown, raising the attack/hit events.
- **Proxy**: faces `FacingYaw` (Mass-authoritative), so a stopped soldier in melee still faces its enemy.

## Architecture Notes

- Pure data-oriented: behavior lives in Mass Processors over packed fragments; no per-soldier Actor,
  Tick, or independent brain (ARCHITECTURE.md 7.4 / 8.3 / 18.3; CLAUDE.md "simple local rules").
- Hierarchy preserved: Commander → Squad set the objective; soldiers only do local actions and are
  leashed to that objective — they never run a macroscopic search (ARCHITECTURE.md 8).
- Data-driven tuning via `UAshSoldierBehaviorConfig`, referenced from the unit-type
  `UAshMassSoldierConfig`; processors fall back to sane defaults when no config is assigned.
- The collision fixes deliberately avoid per-soldier physics collision (the wrong tool at army
  scale): spacing is a same-team steering force, ground placement is an LOD-bounded trace.

## Testing / Verification

- Code review complete. **Build is PENDING** — it was attempted but blocked because the UE editor is
  open with Live Coding active (UBT reported "Unable to build while Live Coding is active"). Build
  with the editor closed:
  `"<UE_5.8>\Engine\Build\BatchFiles\Build.bat" LyraEditor Win64 Development -Project="<...>\AshDraft.uproject" -WaitMutex`
  or press Ctrl+Alt+F11 in the editor to Live-Coding compile.
- PIE verification (user): in a two-army fight on sloped/flat ground, confirm (1) soldiers stand on
  the ground (not half-buried), (2) attacking soldiers face their enemy, (3) a 5+ cluster does not
  shake and the larger side does not shove the smaller while attacking. See the setup guide.

## Known Issues

- Ground conform traces only visible (LOD ≤ MaxConformLOD) soldiers each frame; far soldiers keep
  spawn Z (fine — combat is 2D and they are debug points). If proxy promotion distance is large,
  raise the trace budget or add interval throttling.
- The per-mesh art offset (`UAshSoldierVisualConfig::MeshRelativeLocation.Z`) must still be measured
  per skeletal mesh; ground conform fixes world height, not pelvis-to-feet alignment.
- Separation is same-team only by design; if a future unit should physically block enemies, add a
  thin enemy-vs-enemy min-distance term (kept out here to avoid reintroducing line-shoving).
- Behavior and combat both run on the game thread (the naive single-threaded grids are a Phase 18
  optimization target).

## Next Steps

- Author `DA_<Unit>_Behavior` assets and link them from each `DA_MassSoldierConfig.Behavior`; set the
  per-mesh `MeshRelativeLocation.Z`.
- Optional: integrate the 8-direction `UAshCombatSlotComponent` so soldiers surrounding a hero/general
  take ring slots instead of all converging on one point (the readable-formation upgrade).
- Optional: promote the General/Squad layer to a real Unreal StateTree (PROPOSAL.md §15.2).
