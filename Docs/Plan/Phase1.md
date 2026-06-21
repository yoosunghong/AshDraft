# Phase 1: Gameplay Tags, Input, and Basic Data Setup

Goal: Define the initial gameplay vocabulary and input foundation.

- [x] Add initial AshDraft Gameplay Tags
- [x] Add team tags: Player, Ally, Enemy, Neutral
- [x] Add state tags: Dead, Stunned, Attacking, Capturing
- [x] Add ability tags: BasicAttack, HeavyAttack, Dodge
- [x] Add objective tags: CaptureBase, DefendBase
- [x] Create initial Enhanced Input actions
  - IA_Move, IA_Look_Mouse, IA_Look_Stick (Axis2D); IA_AttackBasic, IA_AttackHeavy,
    IA_Dodge (Digital) created in `/AshDraftCore/Input/`.
- [x] Create initial Input Mapping Context
  - IMC_AshDefault with WASD (Negate/Swizzle), Mouse2D, Gamepad_Right2D, LMB/RMB/Space.
- [x] Create initial Data Asset folders
  - Also created `DA_InputConfig_AshDefault` (`UAshInputConfig`) wiring the actions to
    the native `Ash.InputTag.*` tags.
- [x] Create `Done/DONE_tags_input_setup.md`