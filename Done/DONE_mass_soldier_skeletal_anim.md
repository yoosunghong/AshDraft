# DONE: Mass Soldier Skeletal Meshes + Animations (Data-Driven Unit Types)

## Summary

Rendered Mass soldiers are real skeletal-mesh characters that animate in combat (attack montage
when their strike lands, hit-react montage when damaged), and their entire look is **data-driven
per unit type**. The proxy Blueprint (`B_Soldier_Proxy`) is one generic, empty template; the
skeletal mesh, Animation Blueprint, mesh transform and montages all come from a
`UAshSoldierVisualConfig` data asset applied at runtime. Adding a new unit type
(infantry/archer/cavalry) is pure data authoring — no Blueprint or code changes.

## Architecture

```
DA_<UnitType> (UAshMassSoldierConfig)   = unit type: stats + visual reference
  ├─ MaxHealth / MoveSpeed / Attack…
  └─ Visual ─► DA_<UnitType>_Visual (UAshSoldierVisualConfig)
                 ├─ SkeletalMesh, AnimClass, MeshRelative(Location/Rotation/Scale)
                 └─ AttackMontage, HitReactMontage

Spawner ─references─► DA_<UnitType>; seeds each entity's FAshVisualFragment with the Visual
RepresentationProcessor (per promoted LOD-0 entity):
  Proxy->ConfigureVisual(fragment.Visual)  → dresses the generic proxy at runtime (idempotent)
  Proxy->PlayAttack/HitReactMontage()       → on FAshCombatEventFragment flags
```

Why this shape: a unit's identity lives in **one** place (data), not scattered across a Blueprint
component + actor defaults + a separate stat asset. Stats and visuals are separate assets
(composition) so a look can be reused across stat variants and vice versa.

## Files Changed

- `Public/Mass/AshSoldierVisualConfig.h` — **new** data asset: skeletal mesh, AnimBP class, mesh
  relative transform, attack/hit montages.
- `Public/Mass/AshMassSoldierConfig.h` — now the *unit-type* definition; added a `Visual`
  reference to a `UAshSoldierVisualConfig`.
- `Public/Mass/AshSoldierFragments.h` — `FAshCombatEventFragment` (one-shot attack/hit flags) and
  `FAshVisualFragment` (per-entity link to its unit-type visual set).
- `Public/Mass/AshSoldierProxyActor.h` / `Private/Mass/AshSoldierProxyActor.cpp` — body is a
  `USkeletalMeshComponent`; removed all authored mesh/anim/montage properties; added
  `ConfigureVisual()` (idempotent runtime dressing), `PlayAttack/HitReactMontage()` reading the
  current visual set, movement facing, and montage cleanup on recycle.
- `Private/Mass/AshMassCombatProcessor.cpp` — flags attacker + victim on a landed strike.
- `Private/Mass/AshMassRepresentationProcessor.cpp` — runs after the combat processor; calls
  `ConfigureVisual`, syncs position/facing, plays montages, clears flags each frame.
- `Private/Mass/AshMassSoldierSpawner.cpp` — adds `FAshCombatEventFragment` + `FAshVisualFragment`
  to the archetype and seeds the visual set from `Config->Visual`.

## Implementation Details

- **One generic proxy, many unit types.** Proxies are pooled and reused across entities. The
  representation processor calls `ConfigureVisual` on every promoted entity each frame; it
  early-outs unless the visual set pointer changed, so re-dressing only happens when a pooled proxy
  is handed to a different unit type. Anim class is applied after the mesh so the AnimInstance binds
  to the right skeleton.
- **Event surfacing stays data-oriented.** Combat writes two booleans (attacker + victim); the
  representation processor (which already visits every soldier after movement) consumes them on the
  proxy and clears them, so each event animates exactly once and an off-screen / proxy-capped
  soldier never replays a stale event. `ExecuteAfter` now lists both movement and combat.
- **Bounded cost.** Only LOD-0 soldiers get a proxy (`MaxActiveProxies`), and the mesh uses
  `OnlyTickPoseWhenRendered`, so the number of animating skeletons is capped regardless of army size.

## Architecture Notes

- ECS rules upheld (CLAUDE.md / ARCHITECTURE.md 7): behavior in processors, data in fragments/data
  assets, no per-soldier brain or Actor Tick; Mass stays authoritative, the proxy is a view.
- Data-asset-driven tuning (CLAUDE.md "use Data Assets for tunable values") now covers visuals too.
- No new hard dependencies; the proxy never references the player or AI.

## Testing / Verification

- **Not compiled here** — the UE build runs via LyraEditor (Win64 Development) on the user's
  machine; source-only, auto-compiled by the module.
- PENDING USER (editor + PIE):
  1. In `B_Soldier_Proxy`, leave the `SkeletalMeshComponent` **empty** (no mesh/anim authored).
  2. Create a `DA_<UnitType>_Visual` (`UAshSoldierVisualConfig`): assign skeletal mesh, AnimBP,
     attack + hit montages, and tune the mesh transform (default -90 Z / -90 yaw fits UE mannequins).
  3. On the unit-type asset (`DA_MassSoldierConfig`), set `Visual` to that asset.
  4. PIE: bring soldiers to LOD 0 near the player, let hostile groups engage; confirm skeletal
     soldiers render, face their movement, attack-montage on strike, hit-react on damage.
  5. (Optional) duplicate the assets for a second unit type and point another spawner at it to
     confirm both render distinctly with the same proxy class.

## Known Issues

- The visual pointer is stored per-entity (`FAshVisualFragment`); since it is identical across a
  unit type, a Mass *shared* fragment would dedupe it. Kept as a plain fragment for symmetry with
  the other FAsh* fragments and because one pointer per entity is negligible at PoC scale.
- No death animation: the death processor still removes entities outright. A "play death montage,
  then despawn" path (plus a `DeathMontage` on the visual config) is a clean extension.
- Hit-react restarts on each hit (acceptable for the PoC); blending/queueing is polish.
- Visual assets are hard references; switching to soft refs + async load would trim load-time/memory.

## Next Steps

- Add a `DeathMontage` and a brief death-linger before despawn; optionally promote `FAshVisualFragment`
  to a Mass shared fragment.
