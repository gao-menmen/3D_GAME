"""
Remove-NoWeaponsTag.py - remove the Urban.NoWeapons tag from BP_UrbanTestPawn
so the starting pistol (AddInitialInventory -> ID_Pistol) is granted again.
"""

import unreal

PAWN_PATH = "/UrbanFoundation/Characters/BP_UrbanTestPawn"
TAG = "Urban.NoWeapons"


def main():
    bp = unreal.EditorAssetLibrary.load_asset(PAWN_PATH)
    if not bp:
        raise RuntimeError(f"Cannot load {PAWN_PATH}")

    generated = bp.generated_class()
    cdo = unreal.get_default_object(generated)
    if not cdo:
        raise RuntimeError("No CDO for BP_UrbanTestPawn")

    tags = list(cdo.get_editor_property("tags"))
    unreal.log(f"CDO tags before: {tags}")
    before = len(tags)
    tags = [t for t in tags if str(t) != TAG]
    cdo.set_editor_property("tags", tags)
    unreal.log(f"CDO tags after: {list(cdo.get_editor_property('tags'))}")
    unreal.log(f"Removed {before - len(tags)} occurrence(s) of {TAG}.")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(PAWN_PATH)
    unreal.log("Saved BP_UrbanTestPawn without Urban.NoWeapons tag.")


main()
