# DONE: Mass / Actor Hybrid Representation (Phase 13)

> Status: CODE COMPLETE — NOT yet compiled or verified, and REQUIRES EDITOR WORK (a proxy
> Blueprint with a mesh) before anything is visible. See "Testing / Verification" and
> "Known Issues".

## Summary

Added the hybrid representation layer (ARCHITECTURE.md 13 / 6.3): nearby (LOD 0) Mass
soldiers are promoted to pooled `AAshSoldierProxyActor`s for higher visual fidelity, and
demoted back to pure Mass entities when they move away or die. A bounded pool caps the
active Actor count so only the nearby slice of a large army ever pays Actor cost. Mass
remains the simulation authority; proxies are passive views.

## Files Changed

Created:
- `Public/Mass/AshSoldierProxyActor.h` / `Private/Mass/AshSoldierProxyActor.cpp`
- `Public/Mass/AshSoldierProxyPool.h` / `Private/Mass/AshSoldierProxyPool.cpp`
- `Public/Mass/AshMassRepresentationProcessor.h` /
  `Private/Mass/AshMassRepresentationProcessor.cpp`

Modified:
- `Docs/PLAN.md`, `Docs/Plan/Phase13.md`.

## Implementation Details

- **AAshSoldierProxyActor.** Passive view: a `UStaticMeshComponent` root (no collision, no
  nav, no Tick), hidden when idle. `AssignToEntity` / `ClearAssignment` toggle visibility +
  hold the represented `FMassEntityHandle`; `SyncFromEntity(Position, HealthFraction)` moves
  it (health fraction reserved for future visual feedback); `GetSyncedLocation` reads it back
  for demotion transfer.
- **UAshSoldierProxyPool (UWorldSubsystem).** Owns the bounded pool. `AcquireProxy` returns
  the entity's existing proxy, else a recycled idle one, else a freshly spawned one — or
  `nullptr` when `MaxActiveProxies` is reached. `ReleaseProxy` hides + returns to the idle
  list. `ConfigurePool` receives class + cap from the processor. Destroys all proxies on
  `Deinitialize`. Keyed by `FMassEntityHandle` (a reflected USTRUCT with `GetTypeHash`).
- **UAshMassRepresentationProcessor.** Runs `ExecuteAfter` movement, game thread. Per entity:
  alive + `LODLevel <= PromoteAtOrBelowLOD` → acquire proxy and push position (promotion);
  otherwise, if it has a proxy → copy proxy location back into `FAshMovementFragment` and
  release it (demotion + state transfer, ARCHITECTURE.md 13.3). After the chunk loop it
  sweeps the pool's active set and releases proxies whose entity is no longer valid (killed
  this frame / destroyed). Forwards `ProxyClass`/`MaxActiveProxies` to the pool each frame.

## Architecture Notes

- Promotion/demotion is driven by the Phase 12 LOD level, so "near = detailed, far = cheap"
  falls out of the existing LOD model (ARCHITECTURE.md 13.1/13.2).
- Bounded Actor count = flat Actor cost regardless of army size (ARCHITECTURE.md 13 / 7.5).
- One-way authority (Mass → proxy) keeps the data-oriented sim canonical; the transfer-back
  seam exists for future proxy-authored state.

## Testing / Verification (PENDING USER — do later, all at once)

1. Build the C++.
2. **EDITOR WORK (required for visuals):** create `BP_SoldierProxy` from
   `AAshSoldierProxyActor`, assign a Static Mesh (e.g. a capsule/cube) to `MeshComponent`.
   Set it as the representation processor's `ProxyClass` (Project Settings → Mass processor
   CDO, or via the processor defaults). Without this, the system runs but shows no meshes.
3. PIE with a large spawner near the player: soldiers within `LOD0MaxDistance` should gain
   meshes; walking away should remove them; the active proxy count
   (`UAshSoldierProxyPool::GetActiveProxyCount`) must never exceed `MaxActiveProxies`.
4. Confirm killed near-soldiers' proxies are recycled (count drops, no leaked/ghost actors).

## Known Issues / Scope notes

- **NOT COMPILED** — possible Mass-API nits (see Phase 10 doc).
- **Invisible until a mesh Blueprint is assigned** — by design; flagged so an empty result
  isn't mistaken for a failure.
- Promotion uses LOD level only (distance-only in Phase 12): "visible to camera / near hero
  / recently damaged / in combat-slot range" promotion inputs (ARCHITECTURE.md 13.1) are not
  yet honoured.
- No animation, GAS, or combat-slot integration on proxies yet; they are static-pose views.
- Transfer-back is currently a formality (proxy mirrors Mass); becomes meaningful only if
  proxies later author their own movement/physics.

## Next Steps

Drive proxy animation/orientation from velocity; honour richer promotion inputs; integrate
proxies with the Phase 6 combat-slot system for near-range musou-style melee around the
hero.
