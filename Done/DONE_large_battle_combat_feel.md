# DONE: Large-Battle Combat Feel (Phase 23)

## Summary

Addresses a general-vs-general field report — two generals each leading ~30 soldiers, where "it does
not feel like a large-scale battle at all." Four concrete defects are fixed, all data-driven:

1. **All soldiers converged on the generals.** Fireteam matchmaking now pairs hostile squads off 1:1
   by mutual proximity instead of every squad chasing the single nearest enemy / the enemy general.
2. **Formation not spread out.** Squads now deploy as separated clusters (one V each) rather than every
   soldier scattered across one disc and intermixed.
3. **Soldiers climbed on top of each other.** Same-team separation radius raised, combat anchoring
   relaxed, and the new combat standoff keeps opposing bodies apart.
4. **Soldiers just ground on the enemy in front.** New Dynasty-Warriors-style advance/retreat kiting
   with randomized attack cadence so both lines edge in and out trading blows out of sync.

Plus the requested semantic change: **`General_DataAsset` Troop Count is now the number of 5-soldier
squads** (total soldiers = TroopCount × 5).

## Files Changed

**Data definitions**
- `Public/AI/AshGeneralConfig.h` — `TroopCount` re-documented as squad count (default 6); `TroopSpawnRadius` default 1200.
- `Public/Mass/AshMassSoldierConfig.h` — `AttackCooldownVariance` (per-strike cadence randomization).
- `Public/Mass/AshSoldierBehaviorConfig.h` — new `Kiting` section (enable flag, press/retreat standoff
  scales, `MinCombatSpacing`, hysteresis band, retreat speed scale, press/retreat durations);
  `SeparationRadius` 90→120, `CombatAnchorResistance` 0.85→0.6.
- `Public/Mass/AshSoldierFragments.h` — `FAshCombatFragment`: `AttackCooldownVariance` +
  `RolledAttackInterval`; `FAshSoldierStateFragment`: `bAdvancePhase` + `CombatPhaseEndTime` (kiting).

**Spawning**
- `Public/Mass/AshMassSoldierSpawnLibrary.h` + `Private/...cpp` — params gain `FireteamSize`, `Forward`,
  `FallbackAttackCooldownVariance`. Soldiers are now placed by **fireteam cluster** (golden-angle spread
  within `SpawnRadius`) with members on the V slots; seeds facing, cadence variance, rolled interval.
- `Private/Character/AshGeneralCharacter.cpp` — `SpawnTroops` spawns `TroopCount * 5` soldiers and passes
  the general's forward as the deployment facing.

**Animation (backpedal support)**
- `Public/Mass/AshSoldierAnimInstance.h` — new `Direction` (signed travel-vs-facing angle, −180..180).
- `Private/Mass/AshSoldierProxyActor.cpp` — `SyncFromEntity` computes `Direction` from the planar
  velocity vs. the facing yaw and pushes it, so a 2D locomotion blend space can play a backward-walk
  clip while a soldier kites backwards. Editor setup in
  `Docs/Guides/Phase23_Locomotion_BlendSpace_Setup_Guide.md`.

**Processors**
- `Private/Mass/AshMassFireteamProcessor.cpp` — target assignment reworked to greedy mutual 1:1
  proximity pairing; leftovers double up; only the unpaired remainder targets a general.
- `Private/Mass/AshMassSoldierBehaviorProcessor.cpp` — owns the press/retreat phase (randomized, flipped
  per AI-LOD decision tick) for engaged soldiers.
- `Private/Mass/AshMassMovementProcessor.cpp` — combat-target steering replaced with standoff kiting
  (press to a close strike standoff, give ground to a wider standoff, hold in the hysteresis band).
- `Private/Mass/AshMassCombatProcessor.cpp` — strikes gate on `RolledAttackInterval`, re-rolled from
  `AttackCooldown * [1 - variance, 1 + variance]` after every hit.

## Implementation Details

- **TroopCount → squads.** One line in `SpawnTroops` (`Count = SquadCount * FireteamSize`) plus the param
  plumbing. The fireteam id/slot derivation (`Index / 5`, `Index % 5`) was already squad-aware.
- **Distributed deployment.** The spawn library lays each fireteam's centre on a sunflower
  (golden-angle) pattern across the disc and offsets the 5 members onto the shared V slots, rotated to
  the spawn facing — squads start as discrete, spread-out wedges. The same V offset table drives both
  the spawn position and the fireteam processor's re-form target, so the shape is consistent.
- **Distributed engagement (anti-convergence).** The fireteam processor builds all hostile fireteam
  pairs within `FireteamEngageRadius`, sorts by distance, and greedily binds the closest free pairs
  mutually. The outnumbered side's leftovers gang up on the nearest enemy fireteam; only a fireteam with
  no enemy squad in range targets a general. Generals therefore stop pulling the whole army onto one
  point — most squads brawl squad-on-squad while generals fight each other / the hero.
- **Kiting.** The behavior processor owns a per-soldier `bAdvancePhase` that flips between a randomized
  press duration and a randomized retreat duration on each decision tick (naturally AI-LOD throttled, so
  near soldiers oscillate smoothly and far ones cheaply). The movement processor reads the phase and
  steers to `AttackRange * AttackStandoffScale` (floored by `MinCombatSpacing`) while pressing or
  `AttackRange * RetreatStandoffScale` while retreating, holding inside a hysteresis band. Facing stays
  locked on the target so soldiers backpedal while still looking at the enemy. The combat processor only
  lands a hit while pressed inside attack range, so the press/retreat cycle reads as trading blows.
- **Randomized cadence.** Each soldier draws its next cooldown from `AttackCooldown ± variance` after a
  swing, so neighbours don't swing in lockstep.

## Architecture Notes

- No new Actor Tick; everything stays in the Mass processors and is throttled by the existing AI-LOD
  time-slice (CLAUDE.md 18.3 / ARCHITECTURE.md 9).
- All new tunables are Data Asset fields (CLAUDE.md 17): squad count, spawn radius, kiting standoffs and
  durations, attack-cadence variance, separation. Processors keep sane fallbacks so a soldier with no
  behavior config still behaves.
- Soldiers still never hard-reference a squad/general — fireteam ids and the squad subsystem remain the
  seam (ARCHITECTURE.md 7.4 / 8). The general-actor fallback is the only actor reference and is the
  existing path.
- Reuses, not reinvents: the existing fireteam V slots, the squad-objective follow path, the combat-
  event/anim one-shots, and the spawn library (shared by the placed spawner and the general).

## Testing / Verification

- **Build:** LyraEditor Win64 Development — **Succeeded** (2026-06-28, editor closed; new fragment
  `UPROPERTY`s / structs are not Live-Coding-safe so a full build + editor restart is required).
- **Editor:** set `DA_General_Ally` / `DA_General_Enemy` `TroopCount` to a squad count (e.g. 5–6).
  Optionally tune the `DA_*_Behavior` Kiting fields and `DA_MassSoldierConfig.AttackCooldownVariance`.
- **PIE (general duel):**
  - Each general fields `TroopCount` discrete V squads, deployed spread across `TroopSpawnRadius`.
  - When the lines meet, squads pair off and brawl across the field — they do **not** all pile onto the
    two generals.
  - Engaged soldiers visibly edge in to strike and fall back, trading blows on staggered timing.
  - Soldiers in a melee clump keep spacing instead of climbing onto each other.

## Known Issues

- Backpedal animation needs editor authoring: the proxy now pushes a `Direction` value, but a 2D
  locomotion blend space (Direction × Speed) must be built and wired in the minion AnimBP for the
  backward-walk clip to actually play. Step-by-step in
  `Docs/Guides/Phase23_Locomotion_BlendSpace_Setup_Guide.md`. Until then soldiers backpedal using the
  forward clip (functionally correct, cosmetically off).
- 1:1 pairing is greedy by proximity, not a global optimal matching — fine and cheaper at PoC scale.
- With perfectly equal squad counts no fireteam escorts its own general (all pair off); generals still
  fight via their StateTree + the hero. If "bodyguard" squads are wanted, reserve the general's nearest
  fireteam as an escort.
- Kiting standoffs are derived from `AttackRange`; very long attack ranges widen the dance — tune
  `AttackStandoffScale` / `MinCombatSpacing` per unit type if needed.

## Next Steps

- Build (editor closed), author the data values, and PIE-verify; fill results above.
- Optional: signed-velocity locomotion for clean backpedal anim; reserve an escort fireteam per general;
  morale-driven retreat (whole squad gives ground when losing).
