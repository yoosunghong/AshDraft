# Guide: Authoring Mass Soldier Data Assets (stats + visuals)

This walks through every asset you create and how they connect so a Mass soldier has real stats,
a skeletal mesh, an Animation Blueprint, and attack/hit montages — all data-driven, with no code or
Blueprint changes needed to add new unit types.

## The big picture

```
AshMassSoldierSpawner (placed in the level)
   └─ Config ──► DA_MassSoldierConfig            (UAshMassSoldierConfig = "unit type")
                    ├─ MaxHealth / MoveSpeed / AttackRange / AttackPower / AttackCooldown   (stats)
                    └─ Visual ──► DA_Soldier_Visual   (UAshSoldierVisualConfig = "the look")
                                     ├─ SkeletalMesh
                                     ├─ AnimClass (Anim Blueprint)
                                     ├─ MeshRelativeLocation / Rotation / Scale
                                     ├─ AttackMontage
                                     └─ HitReactMontage

BP_SoldierProxy  (generic, ONE for all unit types — mesh component left EMPTY)
   ▲  dressed at runtime from DA_Soldier_Visual by the representation processor
   └─ already wired as ProxyClass in Config/DefaultGame.ini (no action needed)
```

You author **two** data assets per unit type (a stat asset + a visual asset) and place a spawner.
The proxy Blueprint and the engine ini are already set up.

---

## Step 1 — Create the Visual data asset (the look)

1. Content Browser → go to a sensible folder, e.g. `/AshDraftCore/Data/Soldier/Visual/`
   (create the `Visual` folder if needed).
2. **Add → Miscellaneous → Data Asset.**
3. In the class picker choose **Ash Soldier Visual Config** (`UAshSoldierVisualConfig`).
4. Name it e.g. `DA_Soldier_Infantry_Visual`.
5. Open it and fill in:

   | Field | What to set | Notes |
   |---|---|---|
   | **Skeletal Mesh** | the soldier's skeletal mesh | e.g. the UE/Paragon mannequin mesh |
   | **Anim Class** | the **Animation Blueprint** | must target that mesh's skeleton |
   | **Mesh Relative Location** | `(0, 0, -90)` (default) | drops feet to the ground; tweak per mesh |
   | **Mesh Relative Rotation** | `(0, -90, 0)` (default) | yaw fix so the mesh faces forward (+X) |
   | **Mesh Scale** | `1.0` | uniform scale |
   | **Attack Montage** | montage played when this unit's strike lands | see "Montages" below |
   | **Hit React Montage** | montage played when this unit is hit | see "Montages" below |

> The defaults for location/rotation match the standard UE mannequin. If your soldier ends up
> sunk in the floor or facing sideways, adjust these two fields — that's their whole purpose.

---

## Step 2 — Point the unit-type asset at the visual (and set stats)

You already have `DA_MassSoldierConfig` at `/AshDraftCore/Data/Soldier/`. This is now the
**unit-type** asset: stats **plus** a reference to the look.

1. Open `DA_MassSoldierConfig` (`UAshMassSoldierConfig`).
2. **Visual** → set it to `DA_Soldier_Infantry_Visual` from Step 1. **(This is the key link.)**
3. Tune the stats (defaults shown):

   | Field | Default | Meaning |
   |---|---|---|
   | Max Health | 50 | seeds `FAshHealthFragment` |
   | Move Speed | 350 | cm/s, seeds `FAshMovementFragment` |
   | Attack Range | 150 | cm, when the soldier may strike |
   | Attack Power | 10 | damage per hit |
   | Attack Cooldown | 1.5 | seconds between attacks |

---

## Step 3 — Confirm the proxy Blueprint is a bare template

`BP_SoldierProxy` (`/AshDraftCore/Game/Mass/BP_SoldierProxy`) is the single visible body for **all**
unit types. Its mesh is set at runtime from the visual asset, so:

1. Open `BP_SoldierProxy`.
2. Select its **Mesh Component (SkeletalMeshComponent)**.
3. Make sure **Skeletal Mesh Asset = None** and **Anim Class = None** (leave them EMPTY).
4. Save.

> Don't author a mesh/anim/montage here — that's exactly the redundancy we removed. If you leave a
> mesh assigned, it only acts as a fallback when a unit type has no Visual asset.

`ProxyClass` is already pointed at this Blueprint in `Config/DefaultGame.ini`
(`[/Script/AshDraftCoreRuntime.AshMassRepresentationProcessor] ProxyClass=…BP_SoldierProxy_C`), so
there's nothing else to wire. `MaxActiveProxies=100` caps how many soldiers animate at once
(only LOD-0/near-player soldiers get a proxy).

---

## Step 4 — Place a spawner and assign the unit type

1. Drag an **AshMassSoldierSpawner** into the level (Place Actors → search "AshMassSoldierSpawner").
2. In its Details:

   | Field | Set to |
   |---|---|
   | **Config** | `DA_MassSoldierConfig` (the unit-type asset from Step 2) |
   | **Team Id** | `Ally` / `Enemy` / etc. |
   | **Squad Id** | a number (one squad per spawner) |
   | **Spawn Count** | how many soldiers (e.g. 100–500) |
   | **Spawn Radius** | scatter radius in cm (e.g. 2000) |
   | **bSpawn On Begin Play** | true |
   | **bDraw Debug** | false once meshes render (true draws the old debug points) |

3. Place a second spawner with `Team Id = Enemy` so two groups actually engage and you can see
   attack/hit montages fire.

---

## Step 5 — Montages (attack / hit react)

The two montage fields on the visual asset expect **Anim Montages** (not raw sequences):

1. Find an attack and a hit-react **Animation Sequence** for your skeleton.
2. Right-click each → **Create → Create AnimMontage** (or open the sequence and "Create Montage").
3. Assign the resulting montages to `Attack Montage` / `Hit React Montage` on the visual asset.

That's all that's required for them to play — the Mass combat processor raises the event and the
representation processor calls the montage on the proxy.

---

## Adding more unit types later (zero code)

To add e.g. an archer:

1. Duplicate `DA_Soldier_Infantry_Visual` → `DA_Soldier_Archer_Visual`, swap its mesh/anim/montages.
2. Duplicate `DA_MassSoldierConfig` → `DA_MassSoldierConfig_Archer`, set its `Visual` to the archer
   visual and adjust stats.
3. Place another `AshMassSoldierSpawner`, set its **Config** to `DA_MassSoldierConfig_Archer`.

Same `BP_SoldierProxy`, no Blueprint or C++ changes — one proxy class renders every type.

---

## Verify in PIE

1. Build (LyraEditor, Win64 Development) so the new classes/fields exist.
2. PIE with two opposing spawners near the player.
3. Move the hero close so soldiers reach **LOD 0** (that's when a proxy/mesh appears).
4. Confirm: skeletal soldiers render, turn to face their movement, play the **attack** montage when
   striking and the **hit-react** montage when damaged.

### Troubleshooting

| Symptom | Likely cause |
|---|---|
| Still see debug points, no meshes | Spawner `bDrawDebug` true, or no proxy promotion — get the hero closer; check `ProxyClass` ini line |
| Soldiers visible but T-posing | `Anim Class` not set on the visual asset (or wrong skeleton) |
| Mesh sunk in floor / facing sideways | adjust `Mesh Relative Location` / `Mesh Relative Rotation` |
| No attack/hit animation | montage fields empty, or the field holds a sequence instead of a montage |
| Nothing renders at all | unit-type asset's `Visual` is None, **and** `BP_SoldierProxy` mesh is empty (no fallback) |
```
