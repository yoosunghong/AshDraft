# Guide: Making Paragon Minions animate on Mass soldiers

Follow-up to `Phase15_MassSoldier_DataAssets_Guide.md`. This covers the one thing that guide
assumed already existed: **an Animation Blueprint for the minion skeleton**, plus the attack /
hit-react montages.

## What was actually wrong

I inspected the live project via unreal-mcp. Two data problems and one missing asset:

1. **`DA_MassSoldierConfig.Visual` was `None`.** The unit-type asset never pointed at the visual
   set, so `FAshVisualFragment.Visual` was seeded null and `AAshSoldierProxyActor::ConfigureVisual`
   early-returned — the proxy was never dressed from data at all.
   **→ Fixed via MCP:** it now points at `DA_Soldier_Infantry_Visual` (saved).

2. **`DA_Soldier_Infantry_Visual.AnimClass` is `None`.** A skeletal mesh with no AnimInstance just
   renders its bind/reference pose — i.e. it looks frozen / "T-posed". This is the direct cause of
   "the animations are not visible."

3. **The Paragon Minions pack ships animation *sequences* but no Animation Blueprint.** Confirmed:
   `…/Down_Minions/Animations/Melee/` contains `NonCombat_Idle`, `Combat_JogFwd`, `Attack_A…E`,
   `HitReact_Front/Back/Left/Right`, `Death_A…E`, blendspaces, etc. — but the only AnimBlueprints in
   the whole project are for the UE mannequin and Wukong. There is **nothing to assign** to
   `AnimClass` until you create one. So no, the animation assets are not missing — the *AnimBP* is.

The montage fields (`AttackMontage`, `HitReactMontage`) are also `None`; the sequences exist but
must be wrapped in AnimMontages.

These last steps need the Anim Blueprint editor and the montage editor, which the MCP toolset can't
author (the graph DSL only builds K2 event/function graphs, not AnimGraph nodes; and the Blueprint
factory can't create an AnimBP without a target skeleton). So they're manual — but quick.

## Key facts for the steps below

- Mesh: `/Game/ParagonMinions/Characters/Minions/Down_Minions/Meshes/Minion_Lane_Melee_Core_Dawn`
- Skeleton: `/Game/ParagonMinions/Characters/Minions/Down_Minions/Meshes/Minion_Lane_Core_Skeleton`
- Anim folder: `/Game/ParagonMinions/Characters/Minions/Down_Minions/Animations/Melee/`
- Visual asset to wire up: `/AshDraftCore/Data/Soldier/Visual/DA_Soldier_Infantry_Visual`

> ⚠️ **Movement caveat.** The proxy is repositioned every frame with `SetActorLocation` and has **no
> movement component**, so `GetVelocity()` on it returns 0. A speed-driven blendspace would therefore
> always show idle. For the PoC the simplest correct result is a **constantly-looping locomotion**
> (the soldiers march). See "Optional: real speed-driven locomotion" at the end for the proper fix.

---

## Step 1 — Create the Anim Blueprint

1. Content Browser → `/AshDraftCore/Data/Soldier/Visual/` (or anywhere sensible).
2. **Add → Animation → Animation Blueprint.**
3. In the dialog, pick **Specific Skeleton = `Minion_Lane_Core_Skeleton`** (the minion skeleton
   above). Confirm.
4. Name it `ABP_MinionMelee`. Open it.

## Step 2 — Build a minimal AnimGraph (locomotion + montage slot)

In the **AnimGraph** (double-click it in *My Blueprint*):

1. Drag in a **Sequence Player** node and set its sequence to `Combat_JogFwd` (tick *Loop*). This is
   the always-on march. (Prefer idle? Use `NonCombat_Idle` instead — but marching soldiers read
   better in a battle.)
2. Drag a **Slot** node ('DefaultSlot') — right-click → search "Slot". Connect:
   `Sequence Player → Slot('DefaultSlot') → Output Pose`.
   The slot is **required** for the attack / hit-react montages to be visible; montages play into
   `DefaultSlot` and composite over the locomotion.
3. **Compile** and **Save**.

> That's the whole graph: one looping pose → DefaultSlot → result. Three nodes.

## Step 3 — Create the two montages

The visual asset's montage fields need **AnimMontages**, not raw sequences:

1. In `…/Animations/Melee/`, right-click `Attack_A` → **Create → Create AnimMontage**. Name it
   `AM_Minion_Attack`.
2. Right-click `HitReact_Front` → **Create → Create AnimMontage**. Name it `AM_Minion_HitReact`.
3. Open each montage once and confirm its montage group uses **`DefaultGroup.DefaultSlot`** (the
   default) so it lands in the slot you added in Step 2.

## Step 4 — Wire the visual asset

Open `/AshDraftCore/Data/Soldier/Visual/DA_Soldier_Infantry_Visual` and set:

| Field | Value |
|---|---|
| **Anim Class** | `ABP_MinionMelee` |
| **Attack Montage** | `AM_Minion_Attack` |
| **Hit React Montage** | `AM_Minion_HitReact` |

(Skeletal Mesh is already `Minion_Lane_Melee_Core_Dawn`; the location/rotation defaults `(0,0,-90)` /
`(0,-90,0)` match this skeleton — adjust only if feet sink or facing is off.) **Save.**

> Once these three assets exist, I can finish the wiring (`AnimClass` + both montage fields) via
> unreal-mcp `set_properties` if you'd rather not click — just tell me the asset names you used.

## Step 5 — Verify in PIE

1. Build / Live Coding so C++ is current (no code change was needed here, but the project must run).
2. PIE with two opposing `AshMassSoldierSpawner`s near the player; `bDrawDebug = false`.
3. Get the hero close so soldiers hit **LOD 0** (only then is a proxy spawned and animated).
4. Expect: minions render, march (locomotion loop), turn to face movement, and play the attack /
   hit-react montages when the Mass combat processor raises those events.

---

## Optional: real speed-driven locomotion (proper fix)

If you want idle-vs-run driven by actual movement instead of a constant march, the AnimBP can't read
velocity off the proxy (no movement component). Feed it from Mass instead:

1. In `ABP_MinionMelee`, add a float var `GroundSpeed`; replace the Sequence Player with a
   **BlendSpace Player** over `IdleToRun_A_Combat` (or an idle↔jog 1D blendspace) driven by
   `GroundSpeed`.
2. Push the speed from the proxy. In `AAshSoldierProxyActor::SyncFromEntity` we already receive
   `Velocity`; add a hook that sets `GroundSpeed` on the anim instance each frame (e.g. cast
   `MeshComponent->GetAnimInstance()` to the AnimBP and set the var, or expose a
   `BlueprintImplementableEvent`/native interface). That's a small, contained code change in
   `AshSoldierProxyActor.cpp` — ask and I'll implement it.

The constant-march setup (Steps 1–4) is the zero-code path and is enough to confirm the whole
pipeline animates.

---

# Follow-up: moonwalk, smooth transitions, soldier collision

Three issues reported after the first setup, and what was done about each. **All three needed C++
changes — recompile first (Live Coding `Ctrl+Alt+F11`, or the editor Compile button) before testing.**

## 1. Moonwalk (soldiers face one direction while sliding) — fixed in code

Cause: `AAshSoldierProxyActor` made the **skeletal mesh its own root**. `ConfigureVisual` set the
mesh's `-90` art-correction yaw once, but `SyncFromEntity`'s `SetActorRotation` (facing the movement
direction) then overwrote that same component's rotation every frame — so the art offset was lost and
the mesh faced ~90° off from travel.

Fix (`AshSoldierProxyActor.h/.cpp`): added a plain `USceneComponent RootScene` as the actor root and
attached the mesh under it. Facing is now applied to the root; the mesh keeps its own relative
location/rotation/scale from the visual set. No editor action needed beyond the recompile (the
inherited `BP_SoldierProxy` picks up the new root automatically).

## 2. Smooth Idle → Move → Attack transitions — code hook added, **AnimBP edit required**

The proxy has no movement component, so the AnimBP can't read speed off `GetVelocity()` (always 0).
Added `UAshSoldierAnimInstance` (`Mass/AshSoldierAnimInstance.h`) exposing **`GroundSpeed`** (float)
and **`bIsMoving`** (bool); the proxy writes them from the Mass velocity every frame in
`SyncFromEntity`.

Editor steps on `ABP_MinionMelee`:

1. **Class Settings → Parent Class = `AshSoldierAnimInstance`.** Compile. `GroundSpeed` / `bIsMoving`
   now appear as (read-only) variables.
2. Replace the single looping locomotion node with a **state machine**:
   - State **Idle** → plays `NonCombat_Idle`.
   - State **Locomotion** → a 1D **BlendSpace** (idle↔jog, e.g. built around `IdleToRun_A_Combat` /
     `Combat_JogFwd`) sampled by `GroundSpeed`.
   - Transition Idle→Locomotion when `bIsMoving == true`; Locomotion→Idle when `bIsMoving == false`.
   - Give both transitions a blend time (~0.15–0.25s) so it eases instead of popping.
3. Keep the **`DefaultSlot`** after the state machine (`StateMachine → Slot('DefaultSlot') → Output
   Pose`). Attack / hit-react montages already blend in/out via their own montage blend times, so the
   attack transition is smooth as long as the slot is there.

> Tip: set the montage assets' Blend In/Out to ~0.2s (montage editor) if the attack still snaps.

## 3. Soldiers overlapping — fixed in code (inter-soldier avoidance)

`UAshMassMovementProcessor` now applies a **separation force**: each frame it hashes all soldier
positions into a uniform spatial grid (O(1) neighbour lookups, not O(n²)) and pushes each soldier away
from neighbours inside `SeparationRadius`. Applied even to stopped/in-melee soldiers so clusters
spread; the combined steer + separation speed is clamped to `MoveSpeed`.

Two tunables (config-backed, under `[/Script/AshDraftCoreRuntime.AshMassMovementProcessor]` in
`DefaultGame.ini`, editable on the processor CDO):

| Field | Default | Meaning |
|---|---|---|
| `SeparationRadius` | `90` (cm) | personal-space radius ≈ body diameter. `0` disables avoidance. |
| `SeparationStrength` | `0.6` | push weight vs `MoveSpeed`; higher = spread harder, lower = press the objective. |

`SeparationRadius` (90) is kept below `AttackRange` (150) so soldiers still reach to attack while no
longer interpenetrating. If melee lines jitter, lower `SeparationStrength`; if they still clip, raise
`SeparationRadius`.

> This is steering-based avoidance, not physics collision — soldiers can still briefly overlap when
> funnelled into a chokepoint, then separate. Hard physics collision per soldier is intentionally
> avoided at Mass scale (ARCHITECTURE.md 7.4 / 18.3).
