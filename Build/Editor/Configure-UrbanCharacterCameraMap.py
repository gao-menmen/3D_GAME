import unreal

MAP_PATH = "/UrbanFoundation/Maps/L_UrbanCharacterCameraTest"
EXPERIENCE_CLASS_PATH = "/UrbanFoundation/Experiences/B_UrbanCharacterCameraExperience.B_UrbanCharacterCameraExperience_C"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
MANAGED_TAG = "Urban.CharacterCamera.Managed"
BASE_FLOOR_TAG = "Urban.CharacterCamera.BaseFloor"

STATION_SPECS = [
    (
        "Urban_AimingRestriction",
        unreal.UrbanCharacterCameraTestStationMode.AIMING_RESTRICTION,
        unreal.Vector(700.0, -2200.0, 170.0),
        unreal.Vector(220.0, 180.0, 120.0),
        "Urban.Test.AimingRestriction",
    ),
    (
        "Urban_SprintLane",
        unreal.UrbanCharacterCameraTestStationMode.SPRINTING_RESTRICTION,
        unreal.Vector(0.0, -2200.0, 170.0),
        unreal.Vector(260.0, 180.0, 120.0),
        "Urban.Test.SprintLane",
    ),
    (
        "Urban_TraversalRestriction",
        unreal.UrbanCharacterCameraTestStationMode.TRAVERSAL_RESTRICTION,
        unreal.Vector(1400.0, -2200.0, 170.0),
        unreal.Vector(220.0, 180.0, 120.0),
        "Urban.Test.TraversalTrigger",
    ),
    (
        "Urban_DownedRestriction",
        unreal.UrbanCharacterCameraTestStationMode.DOWNED_RESTRICTION,
        unreal.Vector(2100.0, -2200.0, 170.0),
        unreal.Vector(220.0, 180.0, 120.0),
        "Urban.Test.DownedRestriction",
    ),
    (
        "Urban_ForcedFirstPerson",
        unreal.UrbanCharacterCameraTestStationMode.FORCED_FIRST_PERSON,
        unreal.Vector(2300.0, -1300.0, 170.0),
        unreal.Vector(220.0, 220.0, 120.0),
        "Urban.Test.ForcedFirstPerson",
    ),
    (
        "Urban_DeathReset",
        unreal.UrbanCharacterCameraTestStationMode.DEATH_RESET,
        unreal.Vector(2300.0, 1500.0, 170.0),
        unreal.Vector(240.0, 240.0, 120.0),
        "Urban.Test.DeathReset",
    ),
]

COPIED_GAMEPLAY_CLASS_TOKENS = (
    "/ShooterCore/Blueprint/B_AimAssistTargetTest",
    "/ShooterCore/Blueprint/B_ControlPointVolume",
    "/ShooterCore/Blueprint/B_WeaponSpawner",
    "/Game/Environments/Gameplay/BP_GameplayEffectPad",
)


def actor_tags(actor):
    return {str(tag) for tag in actor.get_editor_property("tags")}


def set_actor_tags(actor, *tags):
    actor.set_editor_property("tags", [unreal.Name(tag) for tag in tags])


def spawn_cube(actor_subsystem, cube_mesh, label, location, scale, semantic_tags, collision_profile):
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor,
        location,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not actor:
        raise RuntimeError(f"Unable to spawn map geometry: {label}")
    actor.set_actor_label(label)
    actor.set_actor_scale3d(scale)
    set_actor_tags(actor, MANAGED_TAG, *semantic_tags)
    mesh = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not mesh:
        raise RuntimeError(f"Spawned geometry lacks StaticMeshComponent: {label}")
    mesh.set_editor_property("static_mesh", cube_mesh)
    mesh.set_collision_profile_name(collision_profile)
    mesh.set_editor_property("generate_overlap_events", False)

    return actor


def spawn_station(actor_subsystem, label, mode, location, extent, semantic_tag):
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.UrbanCharacterCameraTestStation,
        location,
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not actor:
        raise RuntimeError(f"Unable to spawn character-camera station: {label}")
    actor.set_actor_label(label)
    actor.set_trigger_extent(extent)
    actor.set_station_mode(mode)
    set_actor_tags(actor, MANAGED_TAG, semantic_tag)
    return actor


def prepare_base_map(actor_subsystem):
    actors = list(actor_subsystem.get_all_level_actors())
    base_floor = None

    for actor in actors:
        tags = actor_tags(actor)
        if MANAGED_TAG in tags:
            actor_subsystem.destroy_actor(actor)
            continue

        class_path = actor.get_class().get_path_name()
        if any(token in class_path for token in COPIED_GAMEPLAY_CLASS_TOKENS):
            actor_subsystem.destroy_actor(actor)
            continue

        if isinstance(actor, unreal.TextRenderActor):
            actor_subsystem.destroy_actor(actor)
            continue

        if isinstance(actor, unreal.StaticMeshActor):
            if BASE_FLOOR_TAG in tags:
                base_floor = actor
                continue
            location = actor.get_actor_location()
            scale = actor.get_actor_scale3d()
            if (
                actor.get_actor_label() == "Cube"
                and abs(location.x) < 1.0
                and abs(location.y) < 1.0
                and scale.x >= 50.0
                and scale.y >= 50.0
            ):
                base_floor = actor
                set_actor_tags(actor, BASE_FLOOR_TAG)
            else:
                actor_subsystem.destroy_actor(actor)

    if not base_floor:
        raise RuntimeError("Unable to locate the ShooterGym base floor")

    player_starts = sorted(
        [
            actor
            for actor in actor_subsystem.get_all_level_actors()
            if actor.get_class().get_path_name() == "/Script/LyraGame.LyraPlayerStart"
        ],
        key=lambda actor: actor.get_actor_label(),
    )
    if not player_starts:
        raise RuntimeError("Character-camera test map has no LyraPlayerStart")

    start_positions = [
        unreal.Vector(-2450.0, -2350.0, 142.0),
        unreal.Vector(-2450.0, -2150.0, 142.0),
        unreal.Vector(-2250.0, -2350.0, 142.0),
        unreal.Vector(-2250.0, -2150.0, 142.0),
        unreal.Vector(-2650.0, -2350.0, 142.0),
        unreal.Vector(-2650.0, -2150.0, 142.0),
    ]
    for index, player_start in enumerate(player_starts):
        player_start.set_actor_location(start_positions[index % len(start_positions)], False, False)
        player_start.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actual_world = world.get_path_name().split(".")[0] if world else "None"
    if actual_world != MAP_PATH:
        raise RuntimeError(f"Expected editor world {MAP_PATH}, got {actual_world}")

    world_settings = world.get_world_settings()
    if not unreal.UrbanEditorAssetLibrary.set_default_gameplay_experience(
        world_settings,
        EXPERIENCE_CLASS_PATH,
    ):
        raise RuntimeError("Unable to set the Urban character-camera gameplay experience")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    prepare_base_map(actor_subsystem)

    cube_mesh = unreal.EditorAssetLibrary.load_asset(CUBE_PATH)
    if not cube_mesh:
        raise RuntimeError(f"Unable to load cube mesh: {CUBE_PATH}")

    spawn_cube(
        actor_subsystem,
        cube_mesh,
        "Urban_SpawnArea",
        unreal.Vector(-2350.0, -2250.0, 56.0),
        unreal.Vector(6.0, 6.0, 0.12),
        ("Urban.Test.SpawnArea",),
        "NoCollision",
    )
    spawn_cube(
        actor_subsystem,
        cube_mesh,
        "Urban_OpenMovementLane",
        unreal.Vector(-1450.0, -2200.0, 53.0),
        unreal.Vector(12.0, 4.0, 0.05),
        ("Urban.Test.OpenMovementLane",),
        "NoCollision",
    )
    spawn_cube(
        actor_subsystem,
        cube_mesh,
        "Urban_CrouchObstacle",
        unreal.Vector(-600.0, -2200.0, 215.0),
        unreal.Vector(1.0, 5.0, 1.0),
        ("Urban.Test.CrouchObstacle",),
        "BlockAll",
    )
    spawn_cube(
        actor_subsystem,
        cube_mesh,
        "Urban_CameraMuzzleWall",
        unreal.Vector(2750.0, 0.0, 250.0),
        unreal.Vector(1.0, 18.0, 4.0),
        ("Urban.Test.CameraWall", "Urban.Test.MuzzleWall"),
        "BlockAll",
    )

    for station_spec in STATION_SPECS:
        spawn_station(actor_subsystem, *station_spec)

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
        raise RuntimeError("Unable to save configured Urban character-camera map")

    unreal.log(
        "Configured Urban character-camera test map experience, geometry, and six gameplay test stations."
    )


main()