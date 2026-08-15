"""
Add-NoWeaponsTag.py - add the Urban.NoWeapons actor tag to BP_UrbanTestPawn
so the pawn spawns unarmed (FLyraEquipmentList::AddEntry rejects equipment
for pawns carrying this tag). Remove the tag later to restore weapons.
"""

import unreal

PAWN_PATH = "/UrbanFoundation/Characters/BP_UrbanTestPawn"
TAG = "Urban.NoWeapons"


def main():
    bp = unreal.EditorAssetLibrary.load_asset(PAWN_PATH)
    if not bp:
        raise RuntimeError(f"Cannot load {PAWN_PATH}")

    # BP_UrbanTestPawn is a character (AActor). Its tags live on the class
    # default object of the generated class.
    generated = bp.generated_class()
    cdo = unreal.get_default_object(generated)
    if not cdo:
        raise RuntimeError("No CDO for BP_UrbanTestPawn")

    tags = list(cdo.get_editor_property("tags"))
    unreal.log(f"CDO tags before: {tags}")
    if any(str(t) == TAG for t in tags):
        unreal.log("Urban.NoWeapons tag already present; nothing to do.")
        return

    tags.append(unreal.Name(TAG))
    cdo.set_editor_property("tags", tags)
    unreal.log(f"CDO tags after: {list(cdo.get_editor_property('tags'))}")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(PAWN_PATH)
    unreal.log("Saved BP_UrbanTestPawn with Urban.NoWeapons tag.")


main()
