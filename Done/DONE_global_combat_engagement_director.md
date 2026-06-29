---
# DONE: Global Combat + Engagement Deployment Director (Phase 24)

## Summary

Soldier combat no longer forms a single long grinding line when two generals meet. A new **Global
Combat** layer detects when two hostile Generals encounter each other, forms a **Battle**, and runs a
deliberate **engagement deployment algorithm**: every fireteam is assigned a *designated* enemy
fireteam (balanced 1v1 / 1v2, never 1v5) and a deployment slot on a **ring** around the battle centre.
Each squad marches straight to its slot, ignoring every enemy it passes on the way, then duels its
assigned foe there — spreading the clash into a circle of simultaneous squad duels (Dynasty Warriors)
instead of two lines pressed against each other.

## Files Changed

New:
- `Public/AI/AshBattleTypes.h` — `EAshBattleState`, `FAshEngagementAssignment` (enemy fireteam id +
  shared ring slot).
- `Public/AI/AshBattleSubsystem.h` / `Private/AI/AshBattleSubsystem.cpp` — the Engagement Director
  (encounter detection, balanced matching, ring deployment, persistent plan + `Ash.Battle.Debug` draw).

Modified:
- `Public/AI/AshGeneralConfig.h` — new `Ash|General|Battle` tunables: `BattleEncounterRadius`,
  `DuelRingRadius`, `MaxAttackersPerEnemyFireteam`.
- `Public/Mass/AshSoldierFragments.h` — new `EAshFireteamState::Deploying` (enum value only; no struct
  layout change).
- `Public/Mass/AshMassFireteamProcessor.h` / `Private/Mass/AshMassFireteamProcessor.cpp` — consumes the
  battle plan: deploy-to-slot then engage-at-slot, with the Phase 23 greedy proximity kept as the
  out-of-combat fallback. New tunables `BattleSlotArrivalDistance`, `BattleEngageProximity`.
- `Private/Mass/AshMassSoldierBehaviorProcessor.cpp` — reads `FAshFormationFragment`; suppresses local
  hostile sensing while a fireteam is `Deploying` (so squads ignore en-route enemies).
- `Docs/PLAN.md` — Phase 24 entry.

## Implementation Details

**Hierarchy.** The Engagement Director sits above the squad/fireteam layer (ARCHITECTURE.md 8):
Commander → General → **Battle (Global Combat)** → Fireteam → Soldier. It owns no soldiers; it reads
the existing registries — `UAshGeneralSubsystem` (general positions/teams) and `UAshFireteamSubsystem`
(the fireteam decomposition the fireteam processor already publishes each frame) — and writes a plan
the fireteam processor consumes.

**Commitment over reaction.** The plan is recomputed on a 0.5 s timer, never per frame, so each
fireteam *commits* to one designated enemy and one ring slot instead of re-picking the nearest body
every frame (the per-frame greedy pick was the root cause of the static line). `IsInCombat()` /
`GetAssignment(FireteamId)` are the read API.

**Algorithm (`UAshBattleSubsystem::Replan`):**
1. Flatten live, non-dead generals; cluster mutually-hostile generals within `BattleEncounterRadius`
   into battles via union-find.
2. Per battle: centre = mean of participating generals; gather both sides' alive fireteams.
3. **Balanced matching:** greedy 1:1 by mutual proximity; leftover (surplus-side) fireteams double up
   onto the nearest enemy focal still under `MaxAttackersPerEnemyFireteam` (1v1 → 1v2, never 1v5).
4. **Ring deployment:** order duels by their current angle to the centre, then snap them to evenly
   spaced angles at `DuelRingRadius` — a clean circle of duels. Both fireteams in a duel (and any extra
   attacker) share the slot.

**Execution (`UAshMassFireteamProcessor`):** for a battle-assigned fireteam, if it is still far from
its ring slot (and its enemy) it is **Deploying** — formation anchor = the ring slot, no combat target,
so the movement processor steers it straight there and the behavior processor ignores local enemies.
Once within `BattleSlotArrivalDistance` of the slot (or `BattleEngageProximity` of its enemy) it flips
to **Engaged**: anchored on its side of the slot, combat target = the matched enemy slot soldier, and
the existing kiting/attack behavior takes over. Non-battle fireteams keep the Phase 23 path.

**Ignore en route (`UAshMassSoldierBehaviorProcessor`):** while a soldier's fireteam is `Deploying`,
local hostile sensing is skipped entirely, so the squad does not get dragged into side-skirmishes on
the way to its designated enemy (the brief: "ignore enemies along the way and proceed directly").

## Architecture Notes

- **No Actor Tick** (CLAUDE.md 18.3): the director is a single repeating timer; execution is in the
  existing Mass processors.
- **Data-driven** (CLAUDE.md): all battle tuning lives on `UAshGeneralConfig` (the DA the user already
  authors) + the fireteam processor's `config` UPROPERTYs; no hardcoded gameplay numbers (constexpr
  fallbacks only when a general has no config).
- **No new hard dependencies:** the director references only world subsystems and team statics; soldiers
  still hold ids, never object pointers (ARCHITECTURE.md 7.4).
- **Layer boundary preserved:** Unreal StateTree stays on the General; the army-scale work stays a
  data-oriented Mass plan (PROPOSAL.md 15.2/15.3/15.5).
- **Non-destructive:** when no battle is active the plan is empty and the proven Phase 23 fireteam
  matchmaking runs unchanged, so normal marching/skirmishing is unaffected.

## Testing / Verification

Build with the **editor closed** (new subsystem + header `UPROPERTY`s). In PIE:
- Place an Ally and an Enemy `AAshGeneralCharacter`, each with troops, facing off.
- `Ash.Battle.Debug 1` — when the generals close within `BattleEncounterRadius`, a yellow centre, an
  orange ring and red duel slots appear.
- Expect both armies to **split into a ring of 1v1/1v2 squad duels** around the generals, squads to
  **march past intervening enemies** to reach their assigned foe, and the brawl to spread out rather
  than collapse into a line.
- Tune `DuelRingRadius` (circle size), `MaxAttackersPerEnemyFireteam` (how lopsided duels may get),
  and the processor's `BattleSlotArrivalDistance` / `BattleEngageProximity` (deploy→engage hand-off).

## Known Issues

- **Two-sided PoC:** a battle splits into side A (first fireteam's team) vs. everything hostile to it.
  A true 3-faction free-for-all in one battle would need a richer split.
- The deploy→engage hand-off is distance-based; with a very small `DuelRingRadius` relative to army
  size, neighbouring duels can overlap. Tune the ring to the army footprint.
- A wiped enemy fireteam is only reassigned on the next 0.5 s replan; in the gap the surviving fireteam
  briefly falls back to local sensing (reasonable bridging, but a beat of delay).
- Build/PIE verification itself is PENDING USER (UE editor work).

## Next Steps

- Optional: have the two **generals** themselves pair as the central duel of the ring (currently they
  fight via their StateTree `HasThreat`, which already brings them together at the centre).
- Optional: a battle-wide morale/rout state (when a side's fireteam count collapses, the loser breaks
  and the ring dissolves into a pursuit) for an even more dramatic Dynasty-Warriors swing.
- Feed `UAshBattleSubsystem::IsInCombat()` into the telemetry observation for the QA bot.
---
