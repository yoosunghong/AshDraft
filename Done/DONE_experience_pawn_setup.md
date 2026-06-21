# DONE: Experience and Pawn Setup (Phase 2)

## Summary

Created the first playable Lyra-style experience for AshDraft. PIE on the PoC test
map now loads the AshDraft experience and spawns a controllable hero pawn, proving
the experience → pawn-data → game-feature pipeline works end to end.

- `B_Experience_AshDraft_PoC` — experience definition (enables AshDraftCore, sets default pawn data).
- `DA_PawnData_AshHero` — pawn data (pawn class + input config).
- `AshDraftCore` GameFeatureData wired to expose the plugin's primary assets.
- `BaseLevel` World Settings pointed at the experience.

## Files Changed (assets, via the unreal-mcp editor bridge)

- `/AshDraftCore/Experiences/B_Experience_AshDraft_PoC` — **new** Blueprint of
  `ULyraExperienceDefinition`. `GameFeaturesToEnable = ["AshDraftCore"]`,
  `DefaultPawnData = DA_PawnData_AshHero`.
- `/AshDraftCore/PawnData/DA_PawnData_AshHero` — **new** `ULyraPawnData`.
  `PawnClass = /Game/Characters/Heroes/B_Hero_Default`,
  `InputConfig = /Game/Input/InputData_Hero`.
- `/AshDraftCore/AshDraftCore` (GameFeatureData) — **modified** `PrimaryAssetTypesToScan`
  (was empty) → scans `Experiences` (LyraExperienceDefinition), `PawnData` (LyraPawnData),
  `Maps` (Map).
- `/AshDraftCore/Maps/BaseLevel` — World Settings `DefaultGameplayExperience` =
  `B_Experience_AshDraft_PoC` (set manually in-editor).
- `Docs/PLAN.md`, `Docs/Plan/Phase2.md` — progress updated.

Editor-only PIE convenience: `LyraDeveloperSettings.ExperienceOverride` set to the
experience (drives the in-editor spawn test; not required for shipping).

## Implementation Details

- **Reused Lyra classes, no new C++.** Per the experience decision, the experience and
  pawn data are Data-Asset / Blueprint instances of Lyra's `ULyraExperienceDefinition`
  and `ULyraPawnData`. Lyra's `ALyraGameMode` already drives experience loading, so no
  custom GameMode was needed.
- **Experience must be a Blueprint class, not a data-asset instance.**
  `ALyraWorldSettings.DefaultGameplayExperience` is a `TSoftClassPtr<ULyraExperienceDefinition>`,
  so the experience is a (data-only) Blueprint class, following Lyra's `B_*Experience`
  convention rather than the `DA_*` name in ARCHITECTURE.md 4.2.
- **Placeholder pawn.** `B_Hero_Default` (Lyra) is used so spawn is verifiable now; it
  is replaced by `AAshHeroCharacter` in Phase 3, at which point `DA_PawnData_AshHero`
  swaps to the Ash pawn and the project's `DA_InputConfig_AshDefault` (`UAshInputConfig`)
  is consumed by the hero.

## Architecture Notes

- Lyra-style experience-driven setup (ARCHITECTURE 4.2) and PawnData-driven character
  config (4.3); all data-driven, no hardcoded gameplay values.
- Everything lives inside the AshDraftCore Game Feature Plugin; no Lyra core edits.
- No Actor Tick, no new hard dependencies.

## Testing / Verification

- Restarted editor, PIE on `BaseLevel`.
- `LogLyraExperience` shows `B_Experience_AshDraft_PoC` loading (no "couldn't find it"
  fallback), and `B_Hero_Default` spawns as the player pawn. Confirmed by the user.

## Known Issues

- **Critical project gotcha (resolved):** a Game Feature plugin's experiences/pawn-data
  are only discoverable as primary assets if listed in that plugin's GameFeatureData
  `PrimaryAssetTypesToScan`. The project `DefaultGame.ini` rule only covers
  `/Game/System/Experiences`. AshDraftCore's list was empty, causing the experience to
  fall back to `B_LyraDefaultExperience`. Now populated. New scan rules require an editor
  restart to take effect.
- World Settings `DefaultGameplayExperience` (a `TSoftClassPtr`) could not be set through
  the unreal-mcp property bridge — it was set manually in-editor. (Native-class soft refs
  like GameFeatureData's `AssetBaseClass` set fine; only BP soft-class-ptrs fail.)
- Input is the Lyra default (`InputData_Hero`); the Ash input config / Ash hero land in
  Phase 3.
- Running the fallback Lyra default experience triggers Lyra's stock front-end (main menu)
  flow — stock Lyra behavior, unrelated to AshDraft. Now moot since our experience loads.

## Next Steps

- Phase 3: Playable Hero Character — implement `AAshHeroCharacter`, point
  `DA_PawnData_AshHero.PawnClass` at it, and wire `DA_InputConfig_AshDefault`
  (`UAshInputConfig`) for hero movement/look/abilities.
