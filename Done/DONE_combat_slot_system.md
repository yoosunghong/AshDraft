# DONE: 8-Direction Combat Slot System (Phase 6)

## Summary

Implemented the reusable combat slot manager from ARCHITECTURE.md 10: a
`UAshCombatSlotComponent` that carves the ring around a high-value target (hero, enemy
general, base) into N evenly-spaced directional slots and hands them out one per
attacker, so surrounding soldiers spread out readably instead of overlapping into one
clump. Slot geometry is data-driven through a `UAshCombatSlotConfig` data asset, the
ring follows the owner's facing, and a debug visualization renders slot occupancy in
PIE. A dev verification actor (`AAshCombatSlotTestActor`) exercises reservation,
denial, release, and re-request with dummy units.

## Files Changed

C++ (`Source/AshDraftCoreRuntime/`):
- `Public/Combat/AshCombatSlotComponent.h` + `Private/Combat/AshCombatSlotComponent.cpp` (new)
- `Public/Combat/AshCombatSlotConfig.h` (new — data-driven tuning data asset)
- `Public/Verification/AshCombatSlotTestActor.h` + `Private/Verification/AshCombatSlotTestActor.cpp` (new — Phase 6 verification helper)

No `Build.cs` change required: the debug draw helpers and timer manager live in the
already-referenced `Engine` module.

## Implementation Details

- **Slot model:** `EAshCombatSlotState` { Empty, Reserved, Occupied, Attacking,
  Cooldown } (10.2). Each `FAshCombatSlot` holds its state plus a
  `TWeakObjectPtr<AActor>` occupant (weak so a held slot never keeps a dead attacker
  alive). The component owns reservation lifecycle only; Attacking/Cooldown are
  sub-states an attacker stamps via `SetSlotState` (the component drives no combat, 10.3).
- **API (10.3):**
  - `RequestSlot(Requester)` — idempotent; returns the existing slot if already held,
    else the best free slot (set Reserved + occupant), or `INDEX_NONE` when full.
  - `ReleaseSlot(Requester)` / `ReleaseSlotByIndex(int32)` — return a slot to Empty.
  - `GetSlotWorldLocation(int32)` — ring position; slot 0 at owner forward (+X) plus a
    configurable yaw offset, rotated by the owner's yaw so the ring tracks facing.
  - `FindBestAvailableSlot(FromLocation)` — nearest empty slot to a world point (the
    natural slot for an attacker approaching from that side); squared-distance compare.
  - Helpers: `GetSlotForOccupant`, `SetSlotState`, `GetSlotState`, `GetNumSlots`.
- **Data-driven geometry (17):** `UAshCombatSlotConfig` supplies NumSlots, SlotRadius,
  and StartAngleOffsetDegrees. When no config is assigned the component uses inline
  `Fallback*` values (default 8 slots / 175 cm) so a target is functional before an
  author makes a `DA_CombatSlots_*` asset. No hardcoded slot radius/count in logic.
- **Tick policy (18.3):** `bStartWithTickEnabled = false`. Tick is enabled only while
  `bDrawDebugSlots` is true and is used solely to draw the debug ring — slot logic is
  fully event/query-driven. Debug viz colors slots Empty=green, Reserved=yellow,
  Occupied=orange, Attacking=red, Cooldown=blue, with index labels and radial lines.
- **Init safety:** slots are built in `InitializeComponent` and re-checked in
  `BeginPlay`, so early `RequestSlot` calls and editor-added components both get a
  valid ring.

## Architecture Notes

- Narrow, reusable component owned by the target, not by attacker AI (10 / 18.4): no UI
  or AI dependency, no hard reference to the hero/general — attackers interact through
  the generic `AActor*` API, keeping it usable by both Actor soldiers (Phase 8) and
  future Mass soldiers (Phase 9-10).
- Tunables are Data-Asset-driven (17); no hardcoded gameplay numbers in the slot math.
- Weak occupant pointers + index-based release keep it authority/cleanup friendly (15).

## Testing / Verification

`AAshCombatSlotTestActor` (mirrors the Phase 4 `AAshGASTargetDummy` dev-helper pattern):
on BeginPlay it spawns `NumTestRequesters` dummy actors at random angles around itself,
calls `RequestSlot` for each, and logs/on-screen-prints which slot each received (or
"ring full"), then after `ReleaseTestDelay` releases half the slots and re-requests for
the denied dummies to confirm freed slots are re-granted. It auto-enables the debug ring.

Expected results: grants == min(requesters, slots); over-subscription reports denials;
post-release re-grants ≈ released count. **Pending a user C++ build (Live Coding /
editor) + PIE run** to confirm on screen — this environment cannot run UBT/PIE.

## Known Issues

- Not yet attached to the hero or enemy general; that wiring (and authoring a
  `DA_CombatSlots_*` config) happens when soldiers exist to consume slots (Phase 8+).
- `FindBestAvailableSlot` picks the nearest empty slot purely by distance; it does not
  yet account for path/LOS or "slot directly behind a wall" cases.
- Slot ring is flat (owner Z plane); no vertical/terrain projection.
- Verification is synthetic (plain dummy actors), not real soldier AI seeking slots.

## Next Steps

- Phase 7: Base / Stronghold System (`AAshBaseActor`, durability-based capture).
- Later: have soldier AI (Phase 8) and Mass combat processors (Phase 10) request/hold
  combat slots on the hero and enemy general; author a `DA_CombatSlots_*` config asset.
