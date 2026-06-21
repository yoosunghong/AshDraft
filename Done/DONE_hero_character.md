# DONE: Playable Hero Character (Phase 3)

## Summary

Implemented `AAshHeroCharacter`, the first playable general/hero. It is a
self-contained third-person `ACharacter` that owns its camera rig and consumes
the project's data-driven Enhanced Input config (`UAshInputConfig`) built in
Phase 1. The hero ships with the Wukong (Great Sage) skeletal mesh and the
`AnimBP_Wukong_Rigging` animation blueprint, a basic team identity, and
placeholder health values.

Phase 3 scope only: movement, camera, input, placeholder animation, team, and
placeholder health. GAS combat, abilities, and a real AttributeSet are Phase 4.

## Files Changed

- `Source/AshDraftCoreRuntime/Public/Teams/AshTeamTypes.h` — **new** `EAshTeamId`
  enum (Neutral/Player/Ally/Enemy), the lightweight team classification from
  ARCHITECTURE.md 18.1, shared by future units.
- `Source/AshDraftCoreRuntime/Public/Character/AshHeroCharacter.h` — **new**
  `AAshHeroCharacter` declaration.
- `Source/AshDraftCoreRuntime/Private/Character/AshHeroCharacter.cpp` — **new**
  implementation: camera rig, movement config, Enhanced Input binding, Wukong
  mesh/anim, team + health placeholders.
- `Docs/PLAN.md`, `Docs/Plan/Phase3.md` — progress updated.

### Asset configuration pending (editor step)

- `/AshDraftCore/PawnData/DA_PawnData_AshHero` — `PawnClass` must be repointed
  from the placeholder `B_Hero_Default` to `AAshHeroCharacter` after the module
  compiles. (See Testing / Verification.)

## Implementation Details

- **Base class: plain `ACharacter`, not `ALyraCharacter`.** Phase 3 needs only
  movement/camera/input; GAS is Phase 4. Lyra's hero pipeline
  (`ULyraHeroComponent` + `ULyraInputConfig`) would bypass the Phase 1
  `UAshInputConfig` the project deliberately built, and pull in heavy `LyraGame`
  coupling. A plain `ACharacter` still spawns cleanly through the Lyra
  experience/PawnData pipeline because `ULyraPawnData.PawnClass` is
  `TSubclassOf<APawn>`. The hero owns its own camera + input setup, keeping the
  `AshDraftCore` plugin self-contained.
- **Camera:** `USpringArmComponent` (control-rotation, 500u arm) + child
  `UCameraComponent`. The capsule does not rotate with the controller; the
  character orients to movement (`bOrientRotationToMovement`), giving
  screen-relative third-person action movement.
- **Enhanced Input (Lyra-style, data-driven):** Move/Look actions are resolved
  from `UAshInputConfig::FindNativeInputActionForTag` using the native
  `Ash.InputTag.Move` / `Ash.InputTag.Look.Mouse` / `Ash.InputTag.Look.Stick`
  tags. `IMC_AshDefault` is added to the local player's
  `UEnhancedInputLocalPlayerSubsystem` in `PawnClientRestart`. Input asset
  defaults are wired via `ConstructorHelpers` from plugin content
  (`/AshDraftCore/...`) and remain overridable per-instance/Blueprint.
- **Mesh + animation:** set in the constructor via `ConstructorHelpers`
  (`Wukong_GreatSage` skeletal mesh, `AnimBP_Wukong_Rigging_C` anim class),
  mirroring the Unreal third-person template pattern. Mesh is offset to the
  capsule floor and yawed -90° to the UE forward convention.
- **Team identity:** `EAshTeamId TeamId` (defaults to `Player`) with
  Blueprint-pure accessors; mirrors the `Ash.Team.*` tags but is cheap to store
  and compare for the future Mass/AI systems.
- **Health placeholder:** `Health` / `MaxHealth` floats with `GetHealth*`
  accessors, clamped on `BeginPlay`. Explicitly a placeholder — Phase 4 replaces
  these with a GAS AttributeSet.

## Architecture Notes

- Lyra-style: experience/PawnData-driven spawning (4.2/4.3), data-driven input
  via a config asset and Gameplay Tags (4.4/4.5), tunables exposed as
  `EditAnywhere` properties rather than hardcoded (17).
- No Actor Tick (`bCanEverTick = false`); input/movement/anim are event-driven
  (18.3).
- No new hard dependency on `LyraGame`; the plugin owns its hero. Build.cs
  already exposes `EnhancedInput` + `GameplayTags`; no module changes were
  needed (`Camera`/`SpringArm`/`ConstructorHelpers` are in `Engine`).
- Multiplayer-friendly shape: input setup keyed off the local player; no
  cosmetic-only assumptions baked in.

## Testing / Verification

Not yet compiled/run in this environment (no live editor build here). To verify:

1. Compile the `AshDraftCoreRuntime` module (Live Coding or a full build).
2. Open `DA_PawnData_AshHero` and set `PawnClass = AAshHeroCharacter`
   (replacing the placeholder `B_Hero_Default`). Optionally create a
   `B_Hero_Ash` Blueprint subclass instead, if designer-tunable defaults are
   wanted.
3. PIE on `BaseLevel`. Expected: the Wukong hero spawns, WASD moves it
   screen-relative, mouse/right-stick orbits the camera, and the Wukong anim
   blueprint drives locomotion.

## Known Issues

- **PawnData not yet repointed.** `DA_PawnData_AshHero.PawnClass` still targets
  the Phase 2 placeholder `B_Hero_Default`; it must be set to `AAshHeroCharacter`
  after compile (a uasset edit, done in-editor / via the MCP bridge).
- **No GAS yet.** Health/MaxHealth are plain floats and there are no abilities;
  the combat/attack input actions exist but are not bound (Phase 4).
- **Hardcoded content paths.** The Wukong mesh/anim paths live in the hero's
  constructor (template-style). Acceptable for the PoC hero; a later pass could
  move these to a `DA_HeroData` asset (ARCHITECTURE 17) if multiple hero skins
  are needed.
- **Mouse-look vertical sign** inherits the Phase 1 note: `IA_Look_Mouse` has no
  Y-negate modifier, so pitch may feel inverted depending on preference; adjust
  on the input asset if needed.

## Next Steps

- Compile, repoint `DA_PawnData_AshHero.PawnClass`, and verify movement in PIE
  (the one remaining Phase 3 checklist item).
- Proceed to Phase 4: GAS Combat Foundation — add the AbilitySystemComponent and
  an `UAshAttributeSet` (Health/MaxHealth/AttackPower/Defense/Stamina), and wire
  the existing `Ash.InputTag.Attack.*` / `Dodge` actions to gameplay abilities.
  At that point the placeholder health here migrates to the AttributeSet.
