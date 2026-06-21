# Phase 7: Base / Stronghold System

Goal: Implement the first version of capturable battlefield bases.

- [x] Create `AAshBaseActor`
- [x] Add base ownership state
- [x] Add durability value
- [x] Add contested state
- [x] Add defender count tracking
- [x] Add ownership change event
- [x] Add durability recovery rule
- [x] Add base capture UI placeholder
  - Delegates (`OnDurabilityChanged`/`OnOwnershipChanged`/`OnContestedChanged`) +
    BlueprintImplementableEvents (`K2_OnDurabilityChanged`/`K2_OnOwnershipChanged`) +
    on-screen debug. A real widget binds to these later.
- [x] Notify AI systems when ownership changes
  - Via `UAshBaseSubsystem::OnAnyBaseOwnershipChanged` (commander AI subscribes once).
- [x] Verify that base ownership can change during play
  - Presence-driven model is testable now with the existing hero (Player) vs an
    Enemy-owned base. Requires a C++ build + PIE run by the user to confirm on-screen.
- [x] Create `Done/DONE_base_system.md`
