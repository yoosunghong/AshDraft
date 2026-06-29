# Phase 23 — Soldier Locomotion Blend Space (forward / backward walk)

Goal: make a Mass soldier play a **backward-walk** clip while it backpedals during the Dynasty-Warriors
kiting (retreat phase), and a forward clip when it advances, instead of always playing the forward walk.

## Why this is needed

The soldier proxy (`AAshSoldierProxyActor`) has no Character Movement Component, so the AnimBP can't read
`GetVelocity()`. Instead the proxy pushes Mass-authoritative locomotion onto `UAshSoldierAnimInstance`
every frame. As of Phase 23 it pushes three values:

| Field on `UAshSoldierAnimInstance` | Meaning | Use in the AnimGraph |
|---|---|---|
| `GroundSpeed` (float) | planar speed, cm/s (0 .. unit MoveSpeed) | Blend Space **Speed** axis (Y) |
| `Direction` (float) | signed angle travel-vs-facing, −180..180. 0 = forward, ±180 = backpedal, ±90 = strafe | Blend Space **Direction** axis (X) |
| `bIsMoving` (bool) | speed above threshold | gate Idle ↔ Move in a state machine |

`Direction` is the same value `UKismetAnimationLibrary::CalculateDirection` produces, so it behaves like a
standard locomotion setup. A soldier kiting backwards faces the enemy (yaw = toward target) while its
velocity points away → `Direction` ≈ ±180 → the blend space selects the backward clip.

No further C++ is required — this is pure editor authoring.

---

## Step 0 — Confirm the AnimBP parent class

1. Open your minion AnimBP (the one referenced by `UAshSoldierVisualConfig::AnimClass`, e.g. `ABP_AshSoldier`).
2. **Class Settings → Parent Class** must be `AshSoldierAnimInstance`. If it is `AnimInstance`, change it
   and compile. You should now see `GroundSpeed`, `Direction`, `bIsMoving` in the **My Blueprint → Variables**
   (under Inherited) and as pins on `Get` nodes.

> If `AnimClass` on the visual config is empty, the proxy renders the mesh with no animation — set it.

---

## Step 1 — Gather the clips

You need at minimum:

- **Idle** (or Idle is the Speed = 0 sample)
- **Walk/Jog Forward**
- **Walk/Jog Backward**

Optional for a nicer look: **Strafe Left** and **Strafe Right** (Direction ±90).

If you only have a forward walk, you can fake the backward clip by sampling the same animation with a
negative play rate, but a real backward clip reads far better — Lyra/Manny (UE5 Mannequin) ships
`MM_Walk_Fwd`, `MM_Walk_Bwd`, `MM_Walk_Left`, `MM_Walk_Right` you can retarget.

---

## Step 2 — Create the Blend Space

1. Content Browser → **Add → Animation → Blend Space** (2D). Pick the soldier skeleton. Name it
   `BS_AshSoldier_Locomotion`.
2. Open it. **Asset Details → Axis Settings**:
   - **Horizontal Axis** → Name `Direction`, **Minimum −180**, **Maximum 180**, Grid Divisions 4
     (gives sample columns at −180, −90, 0, 90, 180).
   - **Vertical Axis** → Name `Speed`, **Minimum 0**, **Maximum = your unit's MoveSpeed** (default
     `UAshMassSoldierConfig::MoveSpeed` = **350**; use the largest you expect). Grid Divisions 1–2.
3. **Drag clips onto the grid** (X = Direction, Y = Speed):

   | Direction (X) | Speed = 0 | Speed = Max |
   |---|---|---|
   | −180 | Idle | Walk **Backward** |
   | −90  | Idle | Strafe **Left** (or Walk Fwd if no strafe clips) |
   | 0    | Idle | Walk **Forward** |
   | 90   | Idle | Strafe **Right** (or Walk Fwd) |
   | 180  | Idle | Walk **Backward** |

   Put **Idle** on the entire bottom row (Speed = 0). Put **Walk Backward** at BOTH −180 and +180 (the
   axis wraps). If you have no strafe clips, place Walk Forward at −90/0/+90 and Walk Backward at ±180 —
   it still gives correct forward vs. backward.
4. Enable **Target Weight Interpolation** (Asset Details) → set Per Bone / **Weight Speed ≈ 5** so the
   blend doesn't snap when Direction jumps between press/retreat. Recommended.
5. Save.

---

## Step 3 — Drive it from the AnimGraph

Option A — **Blend space directly** (simplest):

```
Event Graph (or AnimGraph thread-safe update):
   GroundSpeed  ─┐
   Direction    ─┤→  Blendspace Player (BS_AshSoldier_Locomotion)  → Output Pose
```

In the AnimGraph, add a **Blendspace Player** node, select `BS_AshSoldier_Locomotion`, and wire:
- **Speed pin** ← `Get GroundSpeed`
- **Direction pin** ← `Get Direction`

Because Idle sits on the Speed = 0 row, the soldier blends to Idle automatically when it holds in the
standoff band (GroundSpeed ≈ 0). You can stop here.

Option B — **State Machine** (cleaner Idle, recommended if you already have one):

- State **Idle**: plays the idle clip. Transition **Idle → Move** when `bIsMoving == true`.
- State **Move**: the Blendspace Player above. Transition **Move → Idle** when `bIsMoving == false`.

Either way, attack / hit-react montages keep working — they are played by the representation processor as
montages on top of this locomotion pose (slot `DefaultSlot`), so make sure your AnimGraph has a **Slot
'DefaultSlot'** node between the locomotion output and the final Output Pose, e.g.:

```
[Locomotion / State Machine] → [Slot 'DefaultSlot'] → [Output Pose]
```

If the montages already play in your current setup, the slot is present — just keep it after the blend
space.

---

## Step 4 — Verify in PIE

1. Build is already done (Phase 23 code). Just Play In Editor with a general duel.
2. Watch a front-line soldier: when it surges in (advance phase) it should walk forward; when it gives
   ground (retreat phase) it should walk **backward while still facing the enemy**; when it holds in the
   band it should idle.
3. If backpedal still plays forward: the AnimBP isn't reading `Direction` (re-check Step 0 parent class
   and the Direction pin), or your backward clip isn't placed at ±180.
4. If it slides without animating: `GroundSpeed` not wired, or Speed axis Max is far above the unit's
   MoveSpeed (so samples never reach the top row) — lower the axis Max to ~MoveSpeed.

---

## Tuning notes

- The **amount** of backpedal is controlled by the kiting data on `UAshSoldierBehaviorConfig`
  (`RetreatStandoffScale`, `RetreatSpeedScale`, `RetreatDuration*`). If the backward walk barely shows,
  raise `RetreatStandoffScale` or `RetreatDuration*` so the give-ground phase lasts longer / travels further.
- `RetreatSpeedScale` (0.7 default) makes backpedal slower than the advance — match the Speed axis so the
  backward clip doesn't over-stride. A retreat speed of 0.7 × 350 = 245 will sample the blend space at
  Speed ≈ 245, between your 0 and Max rows, which looks natural.
- Only **promoted** (near-player, LOD 0) soldiers have a live anim instance, so this only animates
  visible soldiers — exactly where it matters.
