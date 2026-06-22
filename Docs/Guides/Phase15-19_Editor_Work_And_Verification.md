# Phases 15–19: Editor Work & Verification Guide

This is the single reference for everything in Phases 15–19 that **cannot be done in code alone**
(level authoring, asset creation, in-editor measurement, PIE verification), organized by phase.
Each phase has two parts:

- **Editor work** — the concrete steps to do in the Unreal Editor.
- **Verification** — a *layered, diagnostic* checklist. Each layer has a specific signal (a log
  line, an on-screen value, a CVar reaction). The layers are ordered so that **the first one that
  fails tells you where the problem is** — you never have to guess which system broke.

> Prerequisite for all of Phases 16–18: the C++ added this session must be **built**. This was
> **already done this session** — `LyraEditor Win64 Development` → **Succeeded** (2026-06-22),
> `UnrealEditor-AshDraftCoreRuntime.dll` relinked. If you pull fresh, rebuild from the host Lyra
> project with the editor closed (see `Docs/PROJECT_INTEGRATION.md` §5).

## Log categories used as diagnostic signals

Filter the Output Log on these to localize issues quickly:

| Category | Emitted by | Tells you |
| --- | --- | --- |
| `LogAshMatch` | `UAshMatchSubsystem` | match start / end (with Result + Reason codes) |
| `LogAshBase` | `AAshBaseActor` | base capture flips (`'X' captured: old -> new`) |
| `LogAshTelemetry` | `UAshTelemetrySubsystem` | JSON export path, match-result logging |
| `LogAshPerf` | `Ash.Perf.*` commands | spawn totals, perf snapshot |
| `LogAshMassSoldier` | `AAshMassSoldierSpawner` | spawn/respawn counts |

**Match Reason codes** (shown in the on-screen banner as `(reason N)` and in `LogAshMatch`):
`1 = AllBasesCaptured` (victory), `2 = PlayerDeath` (defeat), `3 = MainBaseLost` (defeat).
**Team ids** (in base logs / telemetry): `0 = Neutral, 1 = Player, 2 = Ally, 3 = Enemy`.

---

## §15 — Battlefield PoC Map

**This phase is entirely editor work.** No code deliverable: the supporting classes already exist
(`AAshBaseActor` incl. the new `bIsMainBase`, `AAshMassSoldierSpawner`, the commander/squad/flow-field
subsystems). You are assembling them into a level.

### Editor work

1. **Create the map** (e.g. `Content/Maps/BattlefieldPoC.umap`, or extend the existing `BaseLevel`).
2. **Three bases** — place three `AAshBaseActor` (or your `B_Base` Blueprint):
   - Ally main base: `OwningTeam = Ally` (or `Player`), **`bIsMainBase = true`**.
   - Central base: `OwningTeam = Neutral`, `bIsMainBase = false`.
   - Enemy main base: `OwningTeam = Enemy`, **`bIsMainBase = true`**.
   - (The `bIsMainBase` flag is what Phase 16's defeat condition reads — do not skip it.)
3. **Lanes / chokepoint / wide area** — block out 2–3 lanes connecting the bases, one narrow
   chokepoint, and one open battle area, using static meshes / landscape.
4. **Player spawn** — a `PlayerStart` near the ally base.
5. **Enemy general spawn** — place `AAshEnemyGeneralCharacter` (or `B_EnemyGeneral`) near the enemy base.
6. **Soldier spawn points** — place `AAshMassSoldierSpawner`s: at minimum one Ally (`TeamId=Ally`,
   `SquadId` unique) near the ally base and one Enemy (`TeamId=Enemy`) near the enemy base. Separate
   them so the armies march down lanes (this is also what makes the Phase 14 flow field engage).
7. **NavMesh** — add a `NavMeshBoundsVolume` covering the playable area (the enemy general + actor
   soldiers path on it). Build paths.
8. **Experience** — ensure the level loads the `B_Experience_AshDraft_PoC` experience so the hero/input
   are set up (as on `BaseLevel`).

### Verification (diagnostic order)

| # | Check | Pass signal | If it fails → cause |
| --- | --- | --- | --- |
| 1 | Map opens, paths build | Green nav when you press `P` | No `NavMeshBoundsVolume` / not built |
| 2 | Bases registered with right teams | `Ash.Perf.Report` → `Bases P/A/E/N` matches your placement | Wrong `OwningTeam` on an instance / base didn't BeginPlay |
| 3 | Player spawns & moves | Hero controllable at the ally PlayerStart | Experience/PawnData or PlayerStart misplaced |
| 4 | Enemy general acts | It detects/chases the hero (Phase 5 behavior) | Missing BT/Blackboard or NavMesh (see Phase 5 guide) |
| 5 | Soldiers exist | `Ash.Perf.Report` → `MassSoldiers > 0` | No spawner placed / MassGameplay plugin disabled |
| 6 | Match begins | `LogAshMatch: Match started.` on PIE start | Covered in §16 |

Then run `Verify basic match flow` = play through one capture and confirm §16 reacts.

---

## §16 — Victory, Defeat, and Match Flow

**Code is done** (`UAshMatchSubsystem`, hero `OnHeroDied`, `bIsMainBase`, on-screen result banner).
Remaining work is editor + PIE verification.

### Editor work

1. **Flag the main bases** — on the ally and enemy home base instances set **`bIsMainBase = true`**
   (this is the only required level data for the loss condition). The neutral base stays `false`.
2. **(Optional) Result UMG widget** — the subsystem already shows a debug "VICTORY"/"DEFEAT" banner,
   so this is not required to verify the loop. For a real UI: create a `UUserWidget`, get the
   `UAshMatchSubsystem` (World Subsystem node), and **bind its `OnMatchEnded(Result, Reason)`**
   delegate to show your panel. Do **not** have the widget mutate match/base state (ARCHITECTURE.md 16).
3. No actor needs to be placed for the subsystem — it is a `UWorldSubsystem` (auto-created).

### Verification (diagnostic order — each layer isolates one link)

1. **Subsystem init + match start.** Play. Expect `LogAshMatch: Match started.` immediately.
   - *Fails?* The match subsystem isn't running: confirm you built the new code, and that you are in
     a game world (PIE), not just the editor viewport.
2. **Base events reach the match loop.** Capture/flip any base (walk the hero into a neutral base).
   Expect `LogAshBase: Base 'X' captured: 0 -> 1`.
   - *Base log appears but nothing else?* The link from base→match is broken (the subsystem failed to
     bind `OnAnyBaseOwnershipChanged`); re-check `UAshBaseSubsystem` exists.
   - *No base log at all?* This is a Phase 7 base-capture problem, not match flow.
3. **Victory.** Capture every base for the player side. Expect on-screen **`VICTORY (reason 1)`** and
   `LogAshMatch: Match ended: Result=1 Reason=1`.
   - *All bases look player-owned but no victory?* One base's owner isn't `Player`/`Ally` — check each
     base's team in `Ash.Perf.Report` (`Bases P/A/E/N`); a stray Neutral/Enemy blocks victory.
4. **Defeat — main base lost.** Let the enemy capture a `bIsMainBase` base. Expect **`DEFEAT (reason 3)`**.
   - *Base flips to Enemy but no defeat?* `bIsMainBase` is not set on that base — the single most common
     miss. Re-check the instance flag.
5. **Defeat — player death.** Kill the hero (e.g. stand on the enemy general, or `Ash`-damage it).
   Expect **`DEFEAT (reason 2)`**.
   - *Hero dies but no defeat?* The subsystem didn't bind the hero. Set the log verbosity and look for
     `LogAshMatch: Bound to hero '...'`; if missing, the hero either isn't an `AAshHeroCharacter` or
     spawned after the bind retries gave up (rare — retry runs every 0.5s after BeginPlay).

`Verify that the match can end` (the Phase 16 checklist item) = layers 3, 4, and 5 each producing a
terminal banner. Because each defeat/victory carries a distinct **reason code**, the banner alone
tells you *which* path fired.

---

## §17 — Telemetry and QA Bot Interface

**Code is done** (`UAshTelemetrySubsystem`, observation/action/log types, JSON export, GAS combat hook).
Remaining work is a build + a PIE smoke test.

### Editor work

- None strictly required (the subsystem auto-creates and auto-subscribes). To trigger a JSON dump you
  currently call `UAshTelemetrySubsystem::ExportToFile("")` — easiest from a tiny Blueprint (e.g. a
  level Blueprint key event: *Get World Subsystem → Ash Telemetry Subsystem → Export To File*), or
  from any actor. (A `Ash.QA.Dump` console command is a noted future add.)

### Verification (diagnostic order)

1. **Subsystem present + export works.** Trigger `ExportToFile("")`. Expect
   `LogAshTelemetry: Telemetry exported to '<...>/Saved/AshTelemetry/AshTelemetry_*.json'`.
   - *No log / empty return?* Either the subsystem wasn't created (build issue) or the path is
     unwritable. The log line prints the exact attempted path.
2. **Observation fields are sane.** Open the JSON; check the `observation` object:
   - `player_health` / `player_max_health` match the hero — *wrong?* hero resolution
     (`GetPlayerHero`) — confirm the pawn is an `AAshHeroCharacter`.
   - `owned_bases`/`enemy_bases`/`neutral_bases` match `Ash.Perf.Report` — *wrong?* base subsystem.
   - `current_objective` is non-empty (`Capture:...`/`Defend:...`) — *"None"?* no bases registered.
   - `nearby_enemy_count` rises when enemies approach — *always 0 with Mass armies nearby?* expected:
     Mass entities aren't counted yet (actor soldiers + generals only — see Known Issues).
3. **Combat log populates.** Land a few hero hits, dump again. Expect `combat_log` entries with
   `Source`/`Target`/`Amount` and `EventType` `Damage`/`Kill`.
   - *Damage happens in-game but `combat_log` empty?* The GAS hook
     (`UAshAttributeSet::PostGameplayEffectExecute → Telemetry->LogCombatEvent`) isn't reaching the
     subsystem — confirm the build picked up the attribute-set change.
4. **Base-event log.** Capture a base, dump. Expect a `base_event_log` entry (`OldTeam`/`NewTeam`).
   - *Base flips but no entry?* Not bound to `OnAnyBaseOwnershipChanged`.
5. **Match-result log.** End a match (§16), dump. Expect one `match_result_log` entry with
   `Result`/`Reason`/`MatchDuration`.
   - *Match ends but no entry?* Not bound to `OnMatchEnded` (or the match subsystem is absent).

Each log array maps to exactly one source, so an empty array points straight at the unbound link.

---

## §18 — Performance Validation

**Harness is done** (CVars + console commands + processor wiring). The **measurements** are the
editor work — they must run in PIE on the target hardware. Record numbers in
`Done/DONE_performance_validation.md` (tables are pre-staged there).

### Procedure (editor)

1. Open the battlefield map, fix the camera at a representative spot, and let it run a few seconds.
2. Set population: `Ash.Perf.SpawnSoldiers 100` (then 300, 500, 1000). Confirm with
   `Ash.Perf.Report` → `MassSoldiers`.
3. Read cost: `stat unit` (FPS, Frame/Game/Draw/GPU ms) and `stat mass` / `stat MassProcessor`
   (per-processor cost). `Ash.Perf.Report` gives a quick FPS + counts snapshot alongside.
4. **AI LOD on/off** (at 1000): `Ash.Mass.LOD 1` then `Ash.Mass.LOD 0`; record FPS/Game/Mass ms each.
5. **Time slicing on/off** (at 1000): `Ash.Mass.TimeSlice 1` then `0`; record.
6. Note which line in `stat mass` dominates → that's the bottleneck.

### Verification (that the harness itself is working)

| Check | Pass signal | If it fails → cause |
| --- | --- | --- |
| Spawn command works | `LogAshPerf: ... total live = N`; report shows the count | No spawner placed / Mass plugin off |
| LOD toggle has effect | `Ash.Mass.LOD 0` measurably lowers FPS at high counts | If *no* change, the Mass processors aren't executing — cross-check Phase 10–13 guide §2 ("is Mass ticking"); the rebuild may not have taken |
| Time-slice toggle has effect | `Ash.Mass.TimeSlice 0` measurably lowers FPS | Same as above |

> A toggle that changes nothing is itself the diagnostic: it means the LOD processor isn't running,
> not that the toggle is broken. That points you back to Mass simulation setup, not to Phase 18.

---

## §19 — PoC Final Review

**Verification-only phase** (no code deliverable). Exercise every prior system once in PIE on the
Phase 15 map, then write `Done/DONE_poc_final_review.md` recording pass/fail + notes. Use the table
below; the "Diagnostic signal if it fails" column tells you which phase/system to return to.

| System (Phase) | How to verify | Pass signal | Diagnostic signal if it fails |
| --- | --- | --- | --- |
| Hero movement + combat (3/4/6) | Move + basic attack near the general | Hero moves screen-relative; attack damages | No damage → GAS ability/montage (Phase 4 guide); dummy test `AAshGASTargetDummy` |
| Enemy general combat (5) | Approach the general | It detects → chases → attacks | Idle → BT/Blackboard/NavMesh (Phase 5 guide) |
| Base capture (7) | Stand in a neutral base | `LogAshBase: ... captured`; ownership flips | No flip → presence/team resolution (Phase 7) |
| Soldiers move to objectives (8/10/14) | Watch the armies | They advance down lanes toward target bases | Stuck → movement processor / flow field (Phase 14 guide) |
| Mass simulation (9/10) | `Ash.Perf.Report` | `MassSoldiers > 0`; they fight & die | 0 → spawner/Mass plugin |
| Hierarchical AI (11) | Capture a base | Squads re-target after the flip | No re-target → commander binding (Phase 11) |
| AI LOD + time slicing (12) | LOD debug points; toggle CVars | Color tiers by distance; toggles change FPS | No effect → §18 row above |
| Combat slot system (6) | Soldiers surround the hero/general | Readable 8-direction spacing, no overlap pile | `AAshCombatSlotTestActor` for isolation |
| Victory/defeat loop (16) | Trigger each end condition | Banner with the right reason code (1/2/3) | §16 layered checklist |
| QA-bot interface (17) | Export + open JSON | Observation + 3 logs populate | §17 layered checklist |

### Writing the review

Conclude whether the PoC meets its technical goal (ARCHITECTURE.md 20: *important things precise,
large-scale things cheap*): cite the Phase 18 numbers (soldier count at acceptable FPS, LOD/time-slice
wins), confirm the hybrid Mass+Actor and hierarchical-AI systems interoperate, and list the known
limitations gathered from the DONE docs. Then create `Done/DONE_poc_final_review.md`.

---

## Appendix — quick command reference

```
Ash.Perf.SpawnSoldiers 1000   set every Mass spawner to 1000 and respawn
Ash.Perf.Report               FPS + soldier/base/proxy counts + LOD/TimeSlice state
Ash.Mass.LOD 0 | 1            disable / enable AI LOD (no rebuild)
Ash.Mass.TimeSlice 0 | 1      disable / enable LOD time slicing (no rebuild)
stat unit                     FPS + Frame/Game/Draw/GPU ms
stat mass                     Mass processor cost breakdown
```
