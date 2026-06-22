# Phase 16: Victory, Defeat, and Match Flow

Goal: Complete a simple playable loop.

- [x] Add victory condition: all bases captured
  - `UAshMatchSubsystem::EvaluateBaseConditions` — victory when every registered base is player-side.
- [x] Add defeat condition: player death
  - `AAshHeroCharacter::OnHeroDied` (new) → `UAshMatchSubsystem::HandleHeroDied`.
- [x] Add defeat condition: ally main base captured
  - `AAshBaseActor::bIsMainBase` (new) checked in `EvaluateBaseConditions` (Enemy owns a main base → defeat).
- [x] Add match start event
  - `UAshMatchSubsystem::StartMatch` on world BeginPlay → `OnMatchStarted`.
- [x] Add match end event
  - `UAshMatchSubsystem::EndMatch` → `OnMatchEnded(Result, Reason)` (+ `OnMatchStateChanged`).
- [x] Add simple result UI placeholder
  - Code fallback: on-screen "VICTORY"/"DEFEAT" banner. Real UMG widget binds `OnMatchEnded` (editor work).
- [ ] Verify that the match can end
  - PENDING USER (PIE): see `Docs/Guides/Phase15-19_Editor_Work_And_Verification.md` §16.
- [x] Create `Done/DONE_match_flow.md`