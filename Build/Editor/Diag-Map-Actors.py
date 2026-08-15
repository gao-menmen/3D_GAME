"""
Diag-Map-Actors.py - enumerate every actor in L_UrbanCharacterCameraTest to
see exactly what is on the map (weapons? spawns? stations?).
"""

import unreal

MAP_PATH = "/UrbanFoundation/Maps/L_UrbanCharacterCameraTest"


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actual_world = world.get_path_name().split(".")[0] if world else "None"
    if actual_world != MAP_PATH:
        raise RuntimeError(f"Expected {MAP_PATH}, got {actual_world}")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    unreal.log(f"TOTAL ACTORS: {len(actors)}")
    for actor in actors:
        class_path = actor.get_class().get_path_name()
        label = actor.get_actor_label()
        loc = actor.get_actor_location()
        tags = [str(t) for t in actor.get_editor_property("tags")]
        unreal.log(f"  [{label}] class={class_path} loc=({loc.x:.0f},{loc.y:.0f},{loc.z:.0f}) tags={tags}")


main()
