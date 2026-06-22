# Phase 10–13 Setup: Mass Simulation, Hierarchical AI, LOD, and Hybrid Representation

This single guide covers verifying Phases 10 (movement/combat/death processors),
11 (commander + squads), 12 (AI LOD + time slicing), and 13 (Actor proxies). All four were
implemented in C++ in one pass; compilation and editor/PIE work were intentionally deferred
and are collected here so you can do them all at once.

> IMPORTANT — none of this has been compiled or run yet. The code was written against the
> UE5.8 Mass API as read from engine source. Treat step 1 (build) as the first real test.

## 0. New C++ at a glance

| Phase | Classes |
| --- | --- |
| 10 | `UAshMassMovementProcessor`, `UAshMassCombatProcessor`, `UAshMassDeathProcessor` |
| 11 | `UAshSquadSubsystem`, `UAshCommanderSubsystem`, `UAshMassSquadTrackingProcessor`, `FAshSquadState`/`EAshSquadOrder` |
| 12 | `UAshMassLODProcessor` |
| 13 | `AAshSoldierProxyActor`, `UAshSoldierProxyPool`, `UAshMassRepresentationProcessor` |

No new module dependencies were added (the processors only need `MassEntity`, already
linked from Phase 9). The Phase 9 `AshMassSoldierSpawner` was extended to register its squad.

## 1. Build

1. Close the editor.
2. Regenerate project files if needed, then build **AshDraft** (Win64 Development).
3. If the build fails, it is almost certainly a UE5.8 Mass-API signature nit (the processor
   pattern, `ConfigureQueries(const TSharedRef<FMassEntityManager>&)`, `ForEachEntityChunk`,
   `Context.Defer().DestroyEntity`). Fix in place — the design does not change. See the
   per-file troubleshooting table at the bottom.

## 2. CRITICAL FIRST CHECK — are the processors even running?

Auto-registered `UMassProcessor`s only execute if the **Mass simulation is ticking** in the
world (driven by `UMassSimulationSubsystem` from the Mass Gameplay plugin). This is the most
likely thing to be wrong, so verify it before judging any gameplay logic.

1. PIE with a single `AshMassSoldierSpawner` (Spawn Count ~300, near the player start).
2. If the **green/yellow/orange/red LOD debug points move**, the simulation is running —
   proceed.
3. If the points are **completely static** (only the Phase 9 one-shot draw, which fades after
   60 s), the processors are NOT executing. Scope of the problem = "Mass processing phases
   not running", not the processor code. Things to check:
   - Console: `mass.SimulationTickingEnabled 1`.
   - That **Mass Gameplay** plugin is enabled (it is, from Phase 9).
   - Whether the project needs a `MassSimulation` settings/entry to start the phase manager
     in this world type.
   - As a fallback, we may need to explicitly start simulation from the spawner; note it and
     leave it for a follow-up.

> Tip: turn OFF the Phase 9 spawner's `b Draw Debug` (its static 60 s points) to avoid
> confusing them with the Phase 12 per-frame LOD points.

## 3. Phase 10 — movement, combat, death

1. Place **two** `AshMassSoldierSpawner`s so their spawn discs overlap (radius 2000) and are
   within combat `AcquisitionRadius` (1500) of each other:
   - Spawner A: `Team Id = Ally`, `Squad Id = 0`.
   - Spawner B: `Team Id = Enemy`, `Squad Id = 1`.
2. Play. Expected: the two armies converge, soldiers pick nearest hostile targets, trade
   damage, and die (points disappear). The army thins over time.
3. Output Log filter `LogAshMassDeath` (set to Verbose) shows reaped counts.

Tunables (on the processor CDOs — Project Settings → search the processor, or
`Config/DefaultMass.ini`): `AcquisitionRadius`, `ArrivalTolerance`. Soldier stats come from
`UAshMassSoldierConfig` / spawner fallbacks (Phase 9).

## 4. Phase 11 — commander + squads

1. Add at least one **Phase 7 `AshBaseActor`** to the map (ideally one per team plus a
   neutral one in the middle).
2. Play. Each squad (one per spawner `Squad Id`) should advance on the **nearest base its
   team does not own** (its assigned `AttackBase` objective) rather than just colliding at
   the midpoint.
3. Capture a base (drive the Phase 7 capture flow). The commander listens to
   `UAshBaseSubsystem::OnAnyBaseOwnershipChanged` and **re-assigns squad objectives** when a
   base flips — squads should re-target.
4. (Optional) From a Blueprint, call `UAshSquadSubsystem::GetSquadState(SquadId)` and watch
   `AveragePosition` / `AliveCount` update ~2 Hz.

## 5. Phase 12 — AI LOD + time slicing

1. Large spawner (1000–3000) near the player.
2. Confirm LOD colour rings around the player: **green (LOD0) near → red (LOD3) far**, and
   that they shift as you move the player pawn.
3. Far soldiers should fight/decide noticeably less often than near ones (combat is gated by
   `UpdateInterval`).
4. **Performance comparison** (the one explicitly-unchecked plan item): with `stat unit`
   and large counts, compare game-thread time:
   - LOD on (default).
   - LOD effectively off: temporarily set all four `LODUpdateIntervals` to `0` on the LOD
     processor CDO (forces every soldier to fight every frame).
   Turn `bDrawLODDebug` off while measuring (debug draw is O(n)/frame).

LOD thresholds/intervals/batch count are on the `UAshMassLODProcessor` CDO.

## 6. Phase 13 — Actor proxies (REQUIRES editor work to be visible)

1. **Create the proxy Blueprint:** Content Browser → Blueprint Class → parent
   `AshSoldierProxyActor` → name `BP_SoldierProxy`. Select its `MeshComponent` and assign a
   Static Mesh (a cube/capsule is fine) and a small scale.
2. **Assign it:** on the `UAshMassRepresentationProcessor` CDO (Project Settings → Mass /
   `DefaultMass.ini`), set `ProxyClass = BP_SoldierProxy` and a `MaxActiveProxies` cap
   (default 100).
3. Play near a large army: soldiers within `LOD0MaxDistance` gain meshes; walking away
   removes them; killed near-soldiers' proxies are recycled.
4. Confirm the active proxy count never exceeds `MaxActiveProxies`
   (`UAshSoldierProxyPool::GetActiveProxyCount`, exposed to Blueprint).

> Without step 1–2 the representation system still runs but shows nothing extra — the
> default `AshSoldierProxyActor` has no mesh. That is expected, not a failure.

## 7. Consolidated checklist of deferred work

- [x] Build the C++ (Phases 10–13). *(User reported build complete.)*
- [x] Confirm Mass processors actually tick (Section 2). **VERIFIED 2026-06-22 via MCP PIE:**
      `LogAshMassDeath` reaps entities continuously — simulation is ticking, the highest risk
      is cleared.
- [x] Phase 10: two-army battle with deaths. **VERIFIED:** `AshSpawner_Ally` (team 2 / squad 0)
      and `AshSpawner_Enemy` (team 3 / squad 1) each spawned 400 entities; death processor
      reaps continuously as the armies trade damage.
- [~] Phase 11: squads attack nearest enemy base; re-target on capture. **PARTIAL:** subsystems
      initialize and squads register; a `B_Base_Neutral_Mid` was placed at origin. Ownership-
      change path confirmed (`LogAshBase: captured 0 -> 1` — player pawn captured the mid base).
      Still needs a **visual** PIE pass to confirm squads visibly converge on the correct base
      and re-target on flip. NOTE: Mass *soldiers* do not capture bases (only Actor pawns are
      team-tagged in the volume) — this is the known "base-drain on Mass death" stub.
- [ ] Phase 12: LOD rings; perf compare on/off. **NEEDS USER EYES:** the per-frame LOD debug
      points draw in the PIE game window only (not the editor-viewport capture), so colour/ring
      verification and the `stat unit` perf compare must be done by the user.
- [x] Phase 13: make `BP_SoldierProxy` + assign `ProxyClass`. **DONE (this session):**
      `/AshDraftCore/Game/Mass/BP_SoldierProxy` created (parent `AshSoldierProxyActor`,
      `MeshComponent` = engine Cube, scale 0.4/0.4/1.0) and assigned to the
      `UAshMassRepresentationProcessor` CDO `ProxyClass`. **CAVEAT — does not persist:**
      `ProxyClass` is `EditDefaultsOnly` with no `config` specifier, so the CDO assignment is
      lost on editor restart. See Section 8. Capped-promotion still needs a visual PIE pass.
- [ ] Re-check each `Done/DONE_*.md` "Known Issues" for stubbed items (camera/importance
      LOD, base-drain on Mass death, formation offsets).

## 8. `ProxyClass` persistence — RESOLVED (config-backed)

Originally `UAshMassRepresentationProcessor::ProxyClass` was `EditDefaultsOnly` with **no**
`config` specifier, so the CDO assignment reset to `nullptr` on every editor launch.

**Fixed 2026-06-22 (Option 1):** the class is now `UCLASS(config = Game)` and the three
tunables are `UPROPERTY(config, EditDefaultsOnly)`. The values live in the project
`Config/DefaultGame.ini`:

```
[/Script/AshDraftCoreRuntime.AshMassRepresentationProcessor]
PromoteAtOrBelowLOD=0
MaxActiveProxies=100
ProxyClass=/AshDraftCore/Game/Mass/BP_SoldierProxy.BP_SoldierProxy_C
```

> **Requires a rebuild** — adding `config`/`UCLASS(config=...)` changes generated headers, so
> AshDraftCoreRuntime must be recompiled before this takes effect. After that, edit these
> tunables in `DefaultGame.ini` (or the editor saves CDO edits back to it automatically); no
> more per-session re-assignment.

## Troubleshooting

| Symptom | Likely cause / fix |
| --- | --- |
| Build error in `ConfigureQueries` signature | UE5.8 uses `ConfigureQueries(const TSharedRef<FMassEntityManager>&)`; match the engine version if it differs. |
| Build error on `ForEachEntityChunk` lambda | Signature is `[](FMassExecutionContext& Context)`; iterate via `Context.CreateEntityIterator()`. |
| Build error on `Defer().DestroyEntity` | Include `MassCommandBuffer.h`. |
| Soldiers spawn but never move | Section 2 — Mass simulation not ticking (NOT the movement logic). |
| Soldiers move but never fight | Spawners too far apart (increase overlap) or `AcquisitionRadius` too small; or both spawners on the same team. |
| Squads ignore bases | No `AshBaseActor` in the level, or all bases already owned by the squad's team. |
| No proxy meshes | `ProxyClass` unset or its `MeshComponent` has no mesh (Section 6). |
| Proxy count keeps climbing | Sweep not releasing — check entities are actually being destroyed by the death processor. |
