"""
Create AshDraft Phase 1 Enhanced Input assets.

Run inside the Unreal Editor (with the AshDraftCore plugin loaded & compiled):
    Tools > Execute Python Script...  -> select this file
or in the Output Log "Cmd" set to "Python":
    py "<full path>/Scripts/create_input_assets.py"

Creates (idempotent - existing assets are reused, not duplicated):
    /AshDraftCore/Input/IA_Move            (Axis2D)
    /AshDraftCore/Input/IA_Look_Mouse      (Axis2D)
    /AshDraftCore/Input/IA_Look_Stick      (Axis2D)
    /AshDraftCore/Input/IA_AttackBasic     (Digital/bool)
    /AshDraftCore/Input/IA_AttackHeavy     (Digital/bool)
    /AshDraftCore/Input/IA_Dodge           (Digital/bool)
    /AshDraftCore/Input/IMC_AshDefault     (Input Mapping Context)
    /AshDraftCore/Data/Input/DA_InputConfig_AshDefault  (UAshInputConfig)

The IMC key bindings and the input-config tag wiring use the native
Ash.InputTag.* gameplay tags declared in AshGameplayTags.h.
"""

import unreal

INPUT_DIR = "/AshDraftCore/Input"
DATA_INPUT_DIR = "/AshDraftCore/Data/Input"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def _full_path(directory, name):
    return "{}/{}".format(directory, name)


def get_or_create(directory, name, asset_class):
    """Load the asset if it exists, otherwise create it (factory=None -> NewObject)."""
    path = _full_path(directory, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("Reusing existing asset: {}".format(path))
        return unreal.EditorAssetLibrary.load_asset(path)
    asset = asset_tools.create_asset(name, directory, asset_class, None)
    unreal.log("Created asset: {}".format(path))
    return asset


def make_input_action(name, value_type):
    ia = get_or_create(INPUT_DIR, name, unreal.InputAction)
    ia.set_editor_property("value_type", value_type)
    return ia


def tag(tag_name):
    return unreal.GameplayTagLibrary.request_gameplay_tag(unreal.Name(tag_name))


def key(key_name):
    return unreal.Key(key_name)


def negate(x=True, y=True, z=True):
    m = unreal.InputModifierNegate()
    m.set_editor_property("negate_x", x)
    m.set_editor_property("negate_y", y)
    m.set_editor_property("negate_z", z)
    return m


def swizzle_yxz():
    m = unreal.InputModifierSwizzleAxis()
    m.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return m


def mapping(action, key_obj, modifiers=None):
    m = unreal.EnhancedActionKeyMapping()
    m.set_editor_property("action", action)
    m.set_editor_property("key", key_obj)
    if modifiers:
        m.set_editor_property("modifiers", modifiers)
    return m


def make_input_action_struct(action, input_tag):
    s = unreal.AshInputAction()
    s.set_editor_property("input_action", action)
    s.set_editor_property("input_tag", input_tag)
    return s


def main():
    BOOL = unreal.InputActionValueType.BOOLEAN
    AXIS2D = unreal.InputActionValueType.AXIS2_D

    # --- Input Actions ---
    ia_move = make_input_action("IA_Move", AXIS2D)
    ia_look_mouse = make_input_action("IA_Look_Mouse", AXIS2D)
    ia_look_stick = make_input_action("IA_Look_Stick", AXIS2D)
    ia_attack_basic = make_input_action("IA_AttackBasic", BOOL)
    ia_attack_heavy = make_input_action("IA_AttackHeavy", BOOL)
    ia_dodge = make_input_action("IA_Dodge", BOOL)

    # --- Input Mapping Context ---
    imc = get_or_create(INPUT_DIR, "IMC_AshDefault", unreal.InputMappingContext)
    mappings = [
        # Move (WASD) -> Axis2D via Negate/Swizzle (standard template scheme)
        mapping(ia_move, key("W"), [swizzle_yxz()]),
        mapping(ia_move, key("S"), [negate(), swizzle_yxz()]),
        mapping(ia_move, key("A"), [negate()]),
        mapping(ia_move, key("D")),
        # Look
        mapping(ia_look_mouse, key("Mouse2D")),
        mapping(ia_look_stick, key("Gamepad_Right2D")),
        # Combat
        mapping(ia_attack_basic, key("LeftMouseButton")),
        mapping(ia_attack_heavy, key("RightMouseButton")),
        mapping(ia_dodge, key("SpaceBar")),
    ]
    imc.set_editor_property("mappings", mappings)

    # --- Input Config Data Asset (UAshInputConfig) ---
    config = get_or_create(DATA_INPUT_DIR, "DA_InputConfig_AshDefault", unreal.AshInputConfig)
    config.set_editor_property("native_input_actions", [
        make_input_action_struct(ia_move, tag("Ash.InputTag.Move")),
        make_input_action_struct(ia_look_mouse, tag("Ash.InputTag.Look.Mouse")),
        make_input_action_struct(ia_look_stick, tag("Ash.InputTag.Look.Stick")),
    ])
    config.set_editor_property("ability_input_actions", [
        make_input_action_struct(ia_attack_basic, tag("Ash.InputTag.Attack.Basic")),
        make_input_action_struct(ia_attack_heavy, tag("Ash.InputTag.Attack.Heavy")),
        make_input_action_struct(ia_dodge, tag("Ash.InputTag.Dodge")),
    ])

    # --- Save everything ---
    for path in (
        "/AshDraftCore/Input/IA_Move",
        "/AshDraftCore/Input/IA_Look_Mouse",
        "/AshDraftCore/Input/IA_Look_Stick",
        "/AshDraftCore/Input/IA_AttackBasic",
        "/AshDraftCore/Input/IA_AttackHeavy",
        "/AshDraftCore/Input/IA_Dodge",
        "/AshDraftCore/Input/IMC_AshDefault",
        "/AshDraftCore/Data/Input/DA_InputConfig_AshDefault",
    ):
        unreal.EditorAssetLibrary.save_asset(path)

    unreal.log("AshDraft input assets created/updated successfully.")


if __name__ == "__main__":
    main()
