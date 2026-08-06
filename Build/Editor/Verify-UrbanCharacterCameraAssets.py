import unreal

PAWN_PATH = "/UrbanFoundation/Characters/BP_UrbanTestPawn"
PAWN_DATA_PATH = "/UrbanFoundation/Camera/DA_UrbanCamera_Default"
EXPERIENCE_PATH = "/UrbanFoundation/Experiences/B_UrbanCharacterCameraExperience"
MAP_PATH = "/UrbanFoundation/Maps/L_UrbanCharacterCameraTest"
EXPERIENCE_CLASS_PATH = (
    "/UrbanFoundation/Experiences/B_UrbanCharacterCameraExperience."
    "B_UrbanCharacterCameraExperience_C"
)
CAMERA_CLASS_PATH = "/Script/UrbanCore.LyraCameraMode_UrbanPerspective"
WORLD_BODY_TAG = unreal.Name("Urban.WorldBody")

EXPECTED_COMPONENTS = {
    "UrbanCharacterState": "/Script/UrbanCore.UrbanCharacterStateComponent",
    "UrbanViewPolicy": "/Script/UrbanCore.UrbanViewPolicyComponent",
    "UrbanPerspective": "/Script/UrbanCore.UrbanPerspectiveComponent",
    "UrbanPerspectiveInput": "/Script/UrbanCore.UrbanPerspectiveInputComponent",
    "UrbanPerspectivePresentation": "/Script/UrbanCore.UrbanPerspectivePresentationComponent",
}

EXPECTED_SEMANTIC_TAGS = (
    "Urban.Test.SpawnArea",
    "Urban.Test.OpenMovementLane",
    "Urban.Test.CrouchObstacle",
    "Urban.Test.SprintLane",
    "Urban.Test.AimingRestriction",
    "Urban.Test.TraversalTrigger",
    "Urban.Test.DownedRestriction",
    "Urban.Test.ForcedFirstPerson",
    "Urban.Test.DeathReset",
    "Urban.Test.CameraWall",
    "Urban.Test.MuzzleWall",
)

EXPECTED_STATION_MODES = {
    "Urban.Test.AimingRestriction": unreal.UrbanCharacterCameraTestStationMode.AIMING_RESTRICTION,
    "Urban.Test.SprintLane": unreal.UrbanCharacterCameraTestStationMode.SPRINTING_RESTRICTION,
    "Urban.Test.TraversalTrigger": unreal.UrbanCharacterCameraTestStationMode.TRAVERSAL_RESTRICTION,
    "Urban.Test.DownedRestriction": unreal.UrbanCharacterCameraTestStationMode.DOWNED_RESTRICTION,
    "Urban.Test.ForcedFirstPerson": unreal.UrbanCharacterCameraTestStationMode.FORCED_FIRST_PERSON,
    "Urban.Test.DeathReset": unreal.UrbanCharacterCameraTestStationMode.DEATH_RESET,
}


def require_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Required asset is missing or cannot be loaded: {path}")
    return asset


def object_path(value):
    if value is None:
        return "None"
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)


def verify_pawn_blueprint(pawn_bp):
    unreal.BlueprintEditorLibrary.compile_blueprint(pawn_bp)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(pawn_bp)
    found = []
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(data)
        variable_name = unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(data)
        if obj:
            found.append(
                (
                    str(variable_name) if variable_name else "",
                    obj.get_class().get_path_name(),
                )
            )

    problems = []
    for name, expected_class in EXPECTED_COMPONENTS.items():
        matches = [entry for entry in found if entry[1] == expected_class]
        exact = [entry for entry in matches if entry[0] == name]
        if len(exact) != 1 or len(matches) != 1:
            problems.append(
                f"component {name}: expected one {expected_class} named {name}, "
                f"got {matches or 'missing'}"
            )
    if problems:
        raise RuntimeError("Pawn component verification failed: " + "; ".join(problems))

    cdo = unreal.get_default_object(pawn_bp.generated_class())
    meshes = cdo.get_components_by_class(unreal.SkeletalMeshComponent)
    tagged_meshes = [
        mesh
        for mesh in meshes
        if WORLD_BODY_TAG in list(mesh.get_editor_property("component_tags"))
    ]
    if len(tagged_meshes) != 1:
        names = [mesh.get_name() for mesh in tagged_meshes]
        raise RuntimeError(
            "Expected exactly one pawn skeletal mesh tagged Urban.WorldBody; "
            f"found {len(tagged_meshes)} ({names})"
        )


def verify_pawn_data(pawn_data, pawn_bp):
    expected_pawn_class = pawn_bp.generated_class().get_path_name()
    actual_pawn_class = object_path(pawn_data.get_editor_property("pawn_class"))
    if actual_pawn_class != expected_pawn_class:
        raise RuntimeError(
            f"PawnData pawn_class mismatch: expected {expected_pawn_class}, got {actual_pawn_class}"
        )

    actual_camera_class = object_path(
        pawn_data.get_editor_property("default_camera_mode")
    )
    if actual_camera_class != CAMERA_CLASS_PATH:
        raise RuntimeError(
            "PawnData default_camera_mode mismatch: "
            f"expected {CAMERA_CLASS_PATH}, got {actual_camera_class}"
        )


def verify_experience(experience_bp, pawn_data):
    unreal.BlueprintEditorLibrary.compile_blueprint(experience_bp)
    cdo = unreal.get_default_object(experience_bp.generated_class())
    actual_pawn_data = object_path(cdo.get_editor_property("default_pawn_data"))
    expected_pawn_data = pawn_data.get_path_name()
    if actual_pawn_data != expected_pawn_data:
        raise RuntimeError(
            "Experience default_pawn_data mismatch: "
            f"expected {expected_pawn_data}, got {actual_pawn_data}"
        )


def verify_map():
    world = unreal.UnrealEditorSubsystem().get_editor_world()
    actual_world = world.get_path_name().split(".")[0] if world else "None"
    if actual_world != MAP_PATH:
        raise RuntimeError(f"Expected editor world {MAP_PATH}, got {actual_world}")

    world_settings = world.get_world_settings()
    actual_experience = object_path(
        world_settings.get_editor_property("default_gameplay_experience")
    )
    if actual_experience != EXPERIENCE_CLASS_PATH:
        raise RuntimeError(
            "Map default_gameplay_experience mismatch: "
            f"expected {EXPERIENCE_CLASS_PATH}, got {actual_experience}"
        )


def verify_map_route():
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    actors_by_tag = {}
    for actor in actors:
        for tag in actor.get_editor_property("tags"):
            actors_by_tag.setdefault(str(tag), []).append(actor)

    problems = []
    for semantic_tag in EXPECTED_SEMANTIC_TAGS:
        matches = actors_by_tag.get(semantic_tag, [])
        if len(matches) != 1:
            labels = [actor.get_actor_label() for actor in matches]
            problems.append(
                f"tag {semantic_tag}: expected exactly one actor, found {len(matches)} ({labels})"
            )

    stations = [
        actor
        for actor in actors
        if isinstance(actor, unreal.UrbanCharacterCameraTestStation)
    ]
    if len(stations) != len(EXPECTED_STATION_MODES):
        problems.append(
            f"test stations: expected {len(EXPECTED_STATION_MODES)}, found {len(stations)}"
        )

    for semantic_tag, expected_mode in EXPECTED_STATION_MODES.items():
        matches = actors_by_tag.get(semantic_tag, [])
        if len(matches) != 1:
            continue
        station = matches[0]
        if not isinstance(station, unreal.UrbanCharacterCameraTestStation):
            problems.append(
                f"tag {semantic_tag}: actor {station.get_actor_label()} is not a test station"
            )
            continue
        actual_mode = station.get_station_mode()
        if actual_mode != expected_mode:
            problems.append(
                f"tag {semantic_tag}: expected station mode {expected_mode}, got {actual_mode}"
            )

    if problems:
        raise RuntimeError("Map route verification failed: " + "; ".join(problems))

def main():
    pawn_bp = require_asset(PAWN_PATH)
    pawn_data = require_asset(PAWN_DATA_PATH)
    experience_bp = require_asset(EXPERIENCE_PATH)
    require_asset(MAP_PATH)

    verify_pawn_blueprint(pawn_bp)
    verify_pawn_data(pawn_data, pawn_bp)
    verify_experience(experience_bp, pawn_data)
    verify_map()
    verify_map_route()
    unreal.log(
        "PASS: Urban character-camera pawn, PawnData, experience, and map configuration are valid."
    )


main()
