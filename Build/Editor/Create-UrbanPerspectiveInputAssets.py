import unreal

ACTION_FOLDER = "/UrbanFoundation/Input/Actions"
MAPPING_FOLDER = "/UrbanFoundation/Input/Mappings"
PERSPECTIVE_ACTION_PATH = f"{ACTION_FOLDER}/IA_UrbanTogglePerspective"
SHOULDER_ACTION_PATH = f"{ACTION_FOLDER}/IA_UrbanToggleShoulder"
MAPPING_CONTEXT_PATH = f"{MAPPING_FOLDER}/IMC_UrbanPerspective_KBM"


def load_or_create_asset(asset_path, asset_class, factory_class_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset:
        return asset

    package_path, asset_name = asset_path.rsplit("/", 1)
    factory_class = unreal.load_class(None, factory_class_path)
    if not factory_class:
        raise RuntimeError(f"Unable to load factory class: {factory_class_path}")

    factory = unreal.new_object(factory_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        package_path,
        asset_class,
        factory,
    )
    if not asset:
        raise RuntimeError(f"Unable to create asset: {asset_path}")
    return asset


def configure_boolean_pressed_action(action):
    action.modify()
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    pressed_trigger = unreal.new_object(unreal.InputTriggerPressed, outer=action)
    action.set_editor_property("triggers", [pressed_trigger])


def make_key(key_name):
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def main():
    perspective_action = load_or_create_asset(
        PERSPECTIVE_ACTION_PATH,
        unreal.InputAction,
        "/Script/InputEditor.InputAction_Factory",
    )
    shoulder_action = load_or_create_asset(
        SHOULDER_ACTION_PATH,
        unreal.InputAction,
        "/Script/InputEditor.InputAction_Factory",
    )
    mapping_context = load_or_create_asset(
        MAPPING_CONTEXT_PATH,
        unreal.InputMappingContext,
        "/Script/InputEditor.InputMappingContext_Factory",
    )

    configure_boolean_pressed_action(perspective_action)
    configure_boolean_pressed_action(shoulder_action)

    mapping_context.modify()
    mapping_context.unmap_all()
    mapping_context.map_key(perspective_action, make_key("V"))
    mapping_context.map_key(shoulder_action, make_key("Q"))

    for asset in (perspective_action, shoulder_action, mapping_context):
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Unable to save asset: {asset.get_path_name()}")

    unreal.log("Created Urban Spear perspective input actions and keyboard mapping context.")


main()
