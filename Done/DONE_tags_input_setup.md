# DONE: Gameplay Tags, Input, and Basic Data Setup (Phase 1)

## Summary

Established AshDraft's shared gameplay vocabulary and the source-controllable
backbone of the Enhanced Input system, plus the Content folder structure for
data-driven tuning.

- Native Gameplay Tags for team, state, ability, objective, and input.
- `UAshInputConfig` Data Asset class (Lyra-style InputAction <-> input-tag map).
- `Content/Input` and `Content/Data` folder structure with documentation.

## Files Changed

- `Source/AshDraftCoreRuntime/AshDraftCoreRuntime.Build.cs` — added `GameplayTags`
  and `EnhancedInput` public dependencies.
- `Source/AshDraftCoreRuntime/Public/AshGameplayTags.h` — native tag declarations.
- `Source/AshDraftCoreRuntime/Private/AshGameplayTags.cpp` — native tag definitions
  with comments.
- `Source/AshDraftCoreRuntime/Public/Input/AshInputConfig.h` — `UAshInputConfig`
  + `FAshInputAction`.
- `Source/AshDraftCoreRuntime/Private/Input/AshInputConfig.cpp` — tag lookup impl.
- `Content/Input/README.md` — IA_*/IMC authoring spec.
- `Content/Data/README.md` — data asset layout.
- `Content/Data/{Input,Hero,Soldier,Base,AI,Combat}/` — folders (`.gitkeep`).
- `Docs/PLAN.md`, `Docs/Plan/Phase1.md` — progress updated.

## Implementation Details

- **Gameplay Tags:** Declared natively via `UE_DECLARE_GAMEPLAY_TAG_EXTERN` /
  `UE_DEFINE_GAMEPLAY_TAG_COMMENT` in the `AshGameplayTags` namespace. This gives
  C++ literal-free tag references and auto-registers the tags with the tag manager
  so they appear in the editor picker, Blueprints, and Data Assets. Tag names match
  ARCHITECTURE.md 4.4 (`Ash.Team.*`, `Ash.State.*`, `Ash.Ability.*`,
  `Ash.Objective.Base.*`), plus `Ash.InputTag.*` for input routing.
- **Input config:** `UAshInputConfig` is a const `UDataAsset` holding
  `NativeInputActions` (Move/Look — bound directly) and `AbilityInputActions`
  (routed to GAS by tag in a later phase), each pairing a `UInputAction` with an
  input `FGameplayTag`. This is the Lyra pattern and keeps the input-to-action
  mapping data-driven instead of hardcoded.

## Architecture Notes

- Follows the Lyra-style separation: tags are the shared vocabulary; input is
  data-driven through a config asset rather than hardcoded bindings (ARCHITECTURE
  4.4, 4.5, 17).
- No Actor Tick, no hard dependencies introduced. Input tags decouple input
  sources from gameplay/ability consumers (Dependency Policy 18.4).
- Lives entirely inside the `AshDraftCore` Game Feature Plugin; no Lyra core edits.

## Testing / Verification

- Static review of tag macro usage and module dependencies; not yet compiled in
  this environment.
- To verify: build the `AshDraftCoreRuntime` module, then in the editor confirm
  the `Ash.*` tags appear in the Gameplay Tag picker and that a `UAshInputConfig`
  asset can be created under `Content/Data/Input/`.

## Input Assets Created

Authored through the `unreal-mcp` editor bridge (DataAssetTools / ObjectTools)
and saved to disk:

- `/AshDraftCore/Input/IA_Move`, `IA_Look_Mouse`, `IA_Look_Stick` (Axis2D)
- `/AshDraftCore/Input/IA_AttackBasic`, `IA_AttackHeavy`, `IA_Dodge` (Boolean)
- `/AshDraftCore/Input/IMC_AshDefault` — mappings:
  - Move: W (Swizzle YXZ), S (Negate + Swizzle YXZ), A (Negate), D (raw)
  - Look: Mouse2D, Gamepad_Right2D
  - Combat: LeftMouseButton, RightMouseButton, SpaceBar
- `/AshDraftCore/Data/Input/DA_InputConfig_AshDefault` (`UAshInputConfig`) —
  native actions → `Ash.InputTag.Move|Look.Mouse|Look.Stick`; ability actions →
  `Ash.InputTag.Attack.Basic|Attack.Heavy|Dodge`.

`UInputAction` and `UInputMappingContext` both derive from `UDataAsset`, so all
three asset kinds were created via `DataAssetTools.create`. Verified by reading
the IMC mappings and the config's tag wiring back (tags resolved non-empty,
confirming the native tags registered after compile).

## Known Issues

- Mouse-look vertical sign is not yet negated (no Y-only Negate modifier on
  `IA_Look_Mouse`); revisit when the hero consumes look input in Phase 3.
- Other data asset subfolders remain empty (placeholder `.gitkeep`); concrete
  assets arrive in later phases.

## Next Steps

- Proceed to Phase 2: Experience and Pawn Setup. The hero/player-controller setup
  there will consume `DA_InputConfig_AshDefault` and add `IMC_AshDefault`.
