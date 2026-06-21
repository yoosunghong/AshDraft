# Phase 3: Playable Hero Character

Goal: Implement the first playable general character.

- [x] Create `AAshHeroCharacter`
- [x] Add basic movement support
- [x] Add camera support
- [x] Add Enhanced Input binding
- [x] Add placeholder animation setup
  - Uses `AnimBP_Wukong_Rigging` on the `Wukong_GreatSage` skeletal mesh.
- [x] Add basic team identity
  - `EAshTeamId` enum + `TeamId` property (defaults to Player).
- [x] Add basic health value or Attribute placeholder
  - Placeholder `Health`/`MaxHealth` floats; replaced by a GAS AttributeSet in Phase 4.
- [x] Verify player movement in the PoC map
  - Pending: compile `AshDraftCoreRuntime`, set `DA_PawnData_AshHero.PawnClass` = `AAshHeroCharacter`, then PIE on `BaseLevel`.
- [x] Create `Done/DONE_hero_character.md`