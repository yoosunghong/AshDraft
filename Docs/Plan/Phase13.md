# Phase 13: Actor Proxy / Hybrid Representation

Goal: Represent nearby important Mass soldiers with higher fidelity.

- [x] Define Mass-to-Actor promotion rule
  - `UAshMassRepresentationProcessor`: alive entity with `LODLevel <= PromoteAtOrBelowLOD`
    (default LOD 0 = near player) → acquire a pooled proxy.
- [x] Define Actor-to-Mass demotion rule
  - Entity that drops out of that LOD band (or dies) → transfer back + release the proxy.
- [x] Create soldier proxy Actor class
  - `AAshSoldierProxyActor` (passive `UStaticMeshComponent` view; no Tick, no AI, no
    collision).
- [x] Transfer Mass state to proxy
  - `SyncFromEntity(Position, HealthFraction)` each frame (position drives the actor;
    health fraction reserved for future visual feedback).
- [x] Transfer proxy state back to Mass
  - On demotion, `FAshMovementFragment.Position = Proxy->GetSyncedLocation()` before
    release (ARCHITECTURE.md 13.3).
- [x] Limit maximum active proxy count
  - `UAshSoldierProxyPool` caps active proxies at `MaxActiveProxies` (default 100) and
    recycles idle ones; over-cap entities stay as pure Mass.
- [ ] Verify nearby soldiers can be represented with better fidelity
  - PENDING USER: needs compile + editor (make a proxy BP with a mesh) + PIE. See guide.
- [x] Create `Done/DONE_mass_actor_hybrid_representation.md`

## Notes for compile / editor / verification (do later)
- EDITOR WORK REQUIRED: the default `AAshSoldierProxyActor` has **no mesh assigned**, so
  proxies are invisible until you create `BP_SoldierProxy` (set a Static Mesh on
  `MeshComponent`) and assign it to the representation processor's `ProxyClass`
  (Project Settings → Mass, or the processor CDO). Without a mesh the system still runs;
  it just shows nothing extra over the LOD debug points.
- Proxy tuning (`ProxyClass`, `MaxActiveProxies`) lives on the representation processor CDO
  and is forwarded to the pool each frame via `ConfigurePool`.
- Mass remains the authority; the proxy is a pure view, so transfer-back is currently a
  formality (proxy position == fragment position). The seam exists for future proxy-driven
  animation/physics that could write back real state.
- KNOWN LIMITATION: promotion uses LOD level only (which is distance-only in Phase 12), so
  "visible to camera / near hero / recently damaged" promotion inputs (ARCHITECTURE.md
  13.1) are not yet honoured.
