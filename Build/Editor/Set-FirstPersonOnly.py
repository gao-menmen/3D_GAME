"""
Set-FirstPersonOnly.py - set the BP_UrbanTestPawn view-policy component
default to FirstPersonOnly so the player always spawns in forced first
person (the perspective rules engine then rejects any V/Q switch request).
Only the blueprint instance default changes; the C++ default stays
FreeChoice so the ViewPolicyAuthority test keeps passing.
"""

import unreal

PAWN_PATH = "/UrbanFoundation/Characters/BP_UrbanTestPawn"
VIEW_POLICY_CLASS = "/Script/UrbanCore.UrbanViewPolicyComponent"


def main():
    bp = unreal.EditorAssetLibrary.load_asset(PAWN_PATH)
    if not bp:
        raise RuntimeError(f"Cannot load {PAWN_PATH}")

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    found = False
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        if not obj:
            continue
        class_path = obj.get_class().get_path_name()
        if class_path == VIEW_POLICY_CLASS:
            found = True
            before = obj.get_editor_property("policy")
            unreal.log(f"Found view-policy component: {obj.get_name()} policy before={before}")
            obj.set_editor_property("policy", unreal.UrbanViewPolicy.FIRST_PERSON_ONLY)
            after = obj.get_editor_property("policy")
            unreal.log(f"policy after={after}")
            break

    if not found:
        raise RuntimeError("BP_UrbanTestPawn has no UrbanViewPolicyComponent subobject")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(PAWN_PATH)
    unreal.log("Saved BP_UrbanTestPawn with FirstPersonOnly view policy.")


main()
