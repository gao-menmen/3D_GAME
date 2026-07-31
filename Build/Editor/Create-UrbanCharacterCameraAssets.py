import unreal

PAWN_SOURCE = "/ShooterCore/Game/B_Hero_ShooterMannequin"
PAWN_PATH = "/UrbanFoundation/Characters/BP_UrbanTestPawn"
PAWN_DATA_SOURCE = "/ShooterCore/Game/HeroData_ShooterGame"
PAWN_DATA_PATH = "/UrbanFoundation/Camera/DA_UrbanCamera_Default"
EXPERIENCE_SOURCE = "/ShooterCore/Experiences/B_ShooterGame_Elimination"
EXPERIENCE_PATH = "/UrbanFoundation/Experiences/B_UrbanCharacterCameraExperience"
MAP_SOURCE = "/ShooterCore/Maps/L_ShooterGym"
MAP_PATH = "/UrbanFoundation/Maps/L_UrbanCharacterCameraTest"

COMPONENTS = [
    ("UrbanCharacterState", "/Script/UrbanCore.UrbanCharacterStateComponent"),
    ("UrbanViewPolicy", "/Script/UrbanCore.UrbanViewPolicyComponent"),
    ("UrbanPerspective", "/Script/UrbanCore.UrbanPerspectiveComponent"),
    ("UrbanPerspectiveInput", "/Script/UrbanCore.UrbanPerspectiveInputComponent"),
    ("UrbanPerspectivePresentation", "/Script/UrbanCore.UrbanPerspectivePresentationComponent"),
]


def duplicate_or_load(source_path, target_path):
    asset = unreal.EditorAssetLibrary.load_asset(target_path)
    if asset:
        return asset
    asset = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
    if not asset:
        raise RuntimeError(f"Unable to duplicate {source_path} to {target_path}")
    return asset


def gather_blueprint_subobjects(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    actor_handle = None
    entries = []
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        variable_name = unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data)
        entries.append((handle, obj, str(variable_name) if variable_name else ""))
        if unreal.SubobjectDataBlueprintFunctionLibrary.is_actor(data):
            actor_handle = handle
    if not actor_handle:
        raise RuntimeError(f"Unable to find actor handle for {blueprint.get_path_name()}")
    return subsystem, actor_handle, entries


def ensure_blueprint_component(blueprint, component_name, class_path):
    component_class = unreal.load_class(None, class_path)
    if not component_class:
        raise RuntimeError(f"Unable to load component class: {class_path}")

    subsystem, actor_handle, entries = gather_blueprint_subobjects(blueprint)
    matching = [
        (handle, obj, variable_name)
        for handle, obj, variable_name in entries
        if obj and obj.get_class().get_path_name() == class_path
    ]
    exact = next(
        ((handle, obj) for handle, obj, variable_name in matching if variable_name == component_name),
        None,
    )

    if exact:
        keep_handle, component = exact
    else:
        keep_handle, failure = subsystem.add_new_subobject(
            unreal.AddNewSubobjectParams(actor_handle, component_class, blueprint)
        )
        if not failure.is_empty():
            raise RuntimeError(str(failure))
        subsystem.rename_subobject(handle=keep_handle, new_name=unreal.Text(component_name))
        subsystem.rename_subobject_member_variable(
            bp_context=blueprint,
            handle=keep_handle,
            new_name=unreal.Name(component_name),
        )
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(keep_handle)
        component = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)

    duplicate_handles = [
        handle for handle, _, _ in matching if handle != keep_handle
    ]
    if duplicate_handles:
        deleted = subsystem.delete_subobjects(
            context_handle=actor_handle,
            subobjects_to_delete=duplicate_handles,
            bp_context=blueprint,
        )
        if deleted != len(duplicate_handles):
            raise RuntimeError(
                f"Unable to remove duplicate {component_name} components: "
                f"deleted {deleted} of {len(duplicate_handles)}"
            )
        unreal.log(
            f"Removed {deleted} duplicate {component_name} component(s) from the Urban test pawn."
        )

    return component


def tag_world_mesh(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    meshes = cdo.get_components_by_class(unreal.SkeletalMeshComponent)
    if not meshes:
        raise RuntimeError("Unable to find skeletal mesh component on Urban test pawn CDO")
    mesh = meshes[0]
    tags = list(mesh.get_editor_property("component_tags"))
    tag = unreal.Name("Urban.WorldBody")
    if tag not in tags:
        tags.append(tag)
        mesh.set_editor_property("component_tags", tags)


def main():
    pawn_bp = duplicate_or_load(PAWN_SOURCE, PAWN_PATH)
    for name, class_path in COMPONENTS:
        ensure_blueprint_component(pawn_bp, name, class_path)
    tag_world_mesh(pawn_bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(pawn_bp)

    pawn_data = duplicate_or_load(PAWN_DATA_SOURCE, PAWN_DATA_PATH)
    pawn_data.set_editor_property("pawn_class", pawn_bp.generated_class())
    camera_class = unreal.load_class(None, "/Script/UrbanCore.LyraCameraMode_UrbanPerspective")
    if not camera_class:
        raise RuntimeError("Unable to load Urban perspective camera mode")
    pawn_data.set_editor_property("default_camera_mode", camera_class)

    experience_bp = duplicate_or_load(EXPERIENCE_SOURCE, EXPERIENCE_PATH)
    unreal.BlueprintEditorLibrary.compile_blueprint(experience_bp)
    experience_cdo = unreal.get_default_object(experience_bp.generated_class())
    experience_cdo.set_editor_property("default_pawn_data", pawn_data)
    unreal.BlueprintEditorLibrary.compile_blueprint(experience_bp)

    test_map = duplicate_or_load(MAP_SOURCE, MAP_PATH)

    for asset in (pawn_bp, pawn_data, experience_bp, test_map):
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Unable to save {asset.get_path_name()}")
    del test_map

    unreal.log("Created Urban Spear character-camera integration assets.")


main()
