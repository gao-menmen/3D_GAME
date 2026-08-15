import unreal

MAP_PATH = "/UrbanFoundation/Maps/L_UrbanCharacterCameraTest"
EXPERIENCE_CLASS_PATH = "/UrbanFoundation/Experiences/B_UrbanCharacterCameraExperience.B_UrbanCharacterCameraExperience_C"
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
MANAGED_TAG = "Urban.CharacterCamera.Managed"
BASE_FLOOR_TAG = "Urban.CharacterCamera.BaseFloor"

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


# Perimeter walls enclosing the whole play area. The walls sit just outside
# the four respawn corners and the test corridor so players cannot leave the
# arena. Wall height 400u (4 m), thickness 100u (1 m), length covers the
# full side (5100u). Scale follows the 1:100 cube convention.
WALL_HALF = 2550.0
WALL_HEIGHT = 400.0
WALL_THICK = 100.0


def spawn_perimeter_walls(actor_subsystem, cube_mesh):
    # North and south walls run along X.
    for sign in (1.0, -1.0):
        spawn_cube(
            actor_subsystem,
            cube_mesh,
            "Urban_Wall_North" if sign > 0 else "Urban_Wall_South",
            unreal.Vector(0.0, sign * WALL_HALF, WALL_HEIGHT / 2),
            unreal.Vector(2 * WALL_HALF / 100.0, WALL_THICK / 100.0, WALL_HEIGHT / 100.0),
            ("Urban.Test.Wall",),
            "BlockAll",
        )
    # East and west walls run along Y.
    for sign in (1.0, -1.0):
        spawn_cube(
            actor_subsystem,
            cube_mesh,
            "Urban_Wall_East" if sign > 0 else "Urban_Wall_West",
            unreal.Vector(sign * WALL_HALF, 0.0, WALL_HEIGHT / 2),
            unreal.Vector(WALL_THICK / 100.0, 2 * WALL_HALF / 100.0, WALL_HEIGHT / 100.0),
            ("Urban.Test.Wall",),
            "BlockAll",
        )


# Maze layout designed by the player. The arena (X/Y in [-2550, 2550]) is
# split into an 8x8 grid of cells; each wall segment below runs along a grid
# line between two grid points (gx, gy) with gx, gy in [0, 8]. Grid point
# (0,0) is the south-west corner, (8,8) the north-east corner. Cell size is
# 2550*2/8 = 637.5u (6.375m). Walls are 1m thick and 4m tall, centred on the
# grid line, so adjacent segments meet exactly at grid points and form
# connected corridors.
GRID_CELL = 2.0 * 2550.0 / 8.0  # 637.5
MAZE_WALL_THICK = 1.0
MAZE_WALL_HEIGHT = 4.0
MAZE_SEGMENTS = [
    ((1, 0), (1, 1)), ((2, 1), (2, 2)), ((2, 2), (1, 2)), ((1, 2), (1, 7)),
    ((1, 7), (3, 7)), ((1, 5), (3, 5)), ((1, 4), (3, 4)), ((3, 4), (3, 0)),
    ((3, 7), (3, 6)), ((4, 7), (4, 2)), ((4, 2), (5, 2)), ((6, 0), (6, 5)),
    ((6, 5), (5, 5)), ((7, 1), (7, 6)), ((7, 6), (5, 6)), ((7, 7), (5, 7)),
    ((5, 7), (5, 8)),
]


def spawn_cover(actor_subsystem, cube_mesh):
    index = 0
    for (ax, ay), (bx, by) in MAZE_SEGMENTS:
        index += 1
        if ax == bx:
            # Vertical wall: fixed grid X, runs along grid Y from ay to by.
            length = abs(by - ay) * GRID_CELL
            center = unreal.Vector(
                -2550.0 + ax * GRID_CELL,
                -2550.0 + 0.5 * (ay + by) * GRID_CELL,
                MAZE_WALL_HEIGHT * 50.0,
            )
            scale = unreal.Vector(MAZE_WALL_THICK, length / 100.0, MAZE_WALL_HEIGHT)
        else:
            # Horizontal wall: fixed grid Y, runs along grid X from ax to bx.
            length = abs(bx - ax) * GRID_CELL
            center = unreal.Vector(
                -2550.0 + 0.5 * (ax + bx) * GRID_CELL,
                -2550.0 + ay * GRID_CELL,
                MAZE_WALL_HEIGHT * 50.0,
            )
            scale = unreal.Vector(length / 100.0, MAZE_WALL_THICK, MAZE_WALL_HEIGHT)
        spawn_cube(
            actor_subsystem,
            cube_mesh,
            f"Urban_MazeWall{index}",
            center,
            scale,
            ("Urban.Test.Cover",),
            "BlockAll",
        )


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
    if len(player_starts) < 4:
        raise RuntimeError("Character-camera test map needs at least 4 LyraPlayerStart")

    if len(player_starts) < 10:
        # The map originally ships with 4 corner starts; add the extra ones
        # so the player plus the bots have room to spawn.
        for _ in range(len(player_starts), 10):
            extra_start = actor_subsystem.spawn_actor_from_class(
                unreal.load_class(None, "/Script/LyraGame.LyraPlayerStart"),
                unreal.Vector(0.0, 0.0, 142.0),
                unreal.Rotator(0.0, 0.0, 0.0),
            )
            if not extra_start:
                raise RuntimeError("Unable to spawn extra LyraPlayerStart")
            player_starts.append(extra_start)

    # Ten respawn points, all at the CENTRE of open maze cells (far from
    # every wall so spawns never collide with the maze). Keeping them all in
    # open cell centres prevents bots from piling onto the corner spawns.
    start_positions = [
        unreal.Vector(-1593.75, -1593.75, 142.0),  # cell (1,1)
        unreal.Vector(-956.25, -956.25, 142.0),    # cell (2,2)
        unreal.Vector(-318.75, -318.75, 142.0),    # cell (3,3)
        unreal.Vector(318.75, 318.75, 142.0),      # cell (4,4)
        unreal.Vector(956.25, 956.25, 142.0),      # cell (5,5)
        unreal.Vector(1593.75, 1593.75, 142.0),    # cell (6,6)
        unreal.Vector(-956.25, 1593.75, 142.0),    # cell (2,6)
        unreal.Vector(1593.75, -956.25, 142.0),    # cell (6,2)
        unreal.Vector(-318.75, 1593.75, 142.0),    # cell (3,6)
        unreal.Vector(1593.75, -318.75, 142.0),    # cell (6,3)
    ]
    for index, player_start in enumerate(player_starts[:10]):
        player_start.set_actor_location(start_positions[index], False, False)
        # Face the map centre.
        player_start.set_actor_rotation(unreal.Rotator(0.0, 135.0 + index * 90.0, 0.0), False)

    # Extra player starts beyond the ten we need are removed so respawn
    # selection does not pick from stray points.
    for player_start in player_starts[10:]:
        actor_subsystem.destroy_actor(player_start)


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

    # Use the Urban game mode so the bot/team pipeline is mounted onto the
    # game state (the camera experience lists the bot assets in its bundles
    # but does not attach them, so the bots never spawn without this).
    game_mode_class = unreal.load_class(None, "/Script/UrbanCore.LyraUrbanGameMode")
    if not game_mode_class:
        raise RuntimeError("Unable to load ALyraUrbanGameMode")
    world_settings.set_editor_property("default_game_mode", game_mode_class)

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

    # Note: the crouch obstacle, camera/muzzle wall, and the six gameplay
    # test stations (UrbanCharacterCameraTestStation) were removed per player
    # request. The arena is flat apart from the dense symmetric cover spawned
    # below, plus the ground planes and perimeter walls.

    spawn_perimeter_walls(actor_subsystem, cube_mesh)
    spawn_cover(actor_subsystem, cube_mesh)

    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
        raise RuntimeError("Unable to save configured Urban character-camera map")

    unreal.log(
        "Configured Urban character-camera test map: flat arena with perimeter walls and dense symmetric cover."
    )


main()