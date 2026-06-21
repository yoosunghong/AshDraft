# Phase 6: 8-Direction Combat Slot System

Goal: Prevent surrounding units from overlapping around important combat targets.

- [x] Create `UAshCombatSlotComponent`
- [x] Define 8 directional slots
- [x] Implement slot state model
- [x] Implement `RequestSlot`
- [x] Implement `ReleaseSlot`
- [x] Implement `GetSlotWorldLocation`
- [x] Implement `FindBestAvailableSlot`
- [x] Add debug visualization
- [x] Test slot reservation with dummy units
  - Verification harness `AAshCombatSlotTestActor` created (spawns dummy requesters,
    logs grants/denials, exercises release + re-request). Requires a C++ build +
    PIE run by the user to confirm on-screen.
- [x] Create `Done/DONE_combat_slot_system.md`
