# Phase 2: Experience and Pawn Setup

Goal: Create the first playable Lyra-style experience.

- [x] Create AshDraft PoC Experience Definition
  - `B_Experience_AshDraft_PoC` (Blueprint of `ULyraExperienceDefinition`) in
    `/AshDraftCore/Experiences/`. GameFeaturesToEnable=["AshDraftCore"],
    DefaultPawnData=DA_PawnData_AshHero.
- [x] Create initial PawnData for the playable hero
  - `DA_PawnData_AshHero` (`ULyraPawnData`) in `/AshDraftCore/PawnData/`.
    PawnClass=`B_Hero_Default` (Lyra default; swaps to `AAshHeroCharacter` in Phase 3).
- [x] Create basic GameMode or Experience configuration
  - Uses Lyra's `ALyraGameMode` + experience pipeline; no new GameMode needed.
  - Populated `AshDraftCore` GameFeatureData `PrimaryAssetTypesToScan` so the plugin's
    Experiences/PawnData/Maps are discoverable as primary assets (was empty — caused
    "couldn't find it" fallback).
- [x] Connect input configuration to the playable hero
  - PawnData.InputConfig=`InputData_Hero` for the Phase 2 placeholder pawn.
    Project's `DA_InputConfig_AshDefault` (UAshInputConfig) is consumed by
    `AAshHeroCharacter` in Phase 3.
- [x] Create a temporary test map
  - Reused existing `/AshDraftCore/Maps/BaseLevel`.
- [x] Set the PoC map to load the AshDraft experience
  - `BaseLevel` World Settings → DefaultGameplayExperience = B_Experience_AshDraft_PoC
    (set manually in-editor; the MCP bridge can't serialize the TSoftClassPtr).
- [x] Verify that the player can spawn in the test map
  - PIE verified: experience loads and `B_Hero_Default` spawns (no fallback).
- [x] Create `Done/DONE_experience_pawn_setup.md`
