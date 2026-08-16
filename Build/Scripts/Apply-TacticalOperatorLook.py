"""Apply Urban Spear's original, low-visibility tactical operator palette.

Run with UnrealEditor-Cmd and the PythonScript commandlet. The script updates the
eight stock Manny/Quinn team material instances in place because Lyra's cosmetic
selection and team material swap already reference those assets.
"""

import unreal

ROOT = "/Game/Characters/Heroes/Mannequin/Materials/Instances"

PALETTES = {
    "Blue": {
        "team": unreal.LinearColor(0.025, 0.12, 0.19, 1.0),
        "carbon": unreal.LinearColor(0.018, 0.027, 0.032, 1.0),
        "accent": unreal.LinearColor(0.0, 0.55, 0.72, 1.0),
        "accent_secondary": unreal.LinearColor(0.0, 0.26, 0.38, 1.0),
    },
    "Red": {
        "team": unreal.LinearColor(0.20, 0.025, 0.018, 1.0),
        "carbon": unreal.LinearColor(0.026, 0.021, 0.018, 1.0),
        "accent": unreal.LinearColor(0.78, 0.055, 0.008, 1.0),
        "accent_secondary": unreal.LinearColor(0.55, 0.16, 0.008, 1.0),
    },
}

VECTOR_PARAMETERS = {
    "TeamColor": "team",
    "CarbonfiberTint": "carbon",
    "EmissiveColor": "accent",
    "EmissiveColor2": "accent_secondary",
    "EmissiveColor3": "accent",
    "EdgeGlowColor": "accent_secondary",
}

# Matte paint, rubber and fabric-like response instead of Lyra's bright sci-fi shell.
SCALAR_PARAMETERS = {
    "EmissiveStrength": 1.35,
    "EmissiveStrength2": 2.25,
    "EmissiveStrength3": 3.0,
    "EdgeGlowMagnitude": 0.08,
    "MetalBrighness": 0.42,
    "RubberBrightness": 0.72,
    "PlasticBrightness": 0.68,
    "LineBrightness": 0.72,
    "CarbonFiberAniso": 0.35,
}


def update_material(path: str, palette: dict) -> None:
    material = unreal.load_asset(path)
    if not isinstance(material, unreal.MaterialInstanceConstant):
        raise RuntimeError("Missing material instance: {}".format(path))

    for parameter, palette_key in VECTOR_PARAMETERS.items():
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            material, parameter, palette[palette_key]
        )

    for parameter, value in SCALAR_PARAMETERS.items():
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            material, parameter, value
        )

    unreal.MaterialEditingLibrary.update_material_instance(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Failed to save {}".format(path))
    unreal.log("TACTICAL_LOOK updated {}".format(path))


for body in ("Manny", "Quinn"):
    for slot in ("01", "02"):
        for team, palette in PALETTES.items():
            asset_name = "MI_{}_{}_{}".format(body, slot, team)
            update_material(
                "{}/{}/{}.{}".format(ROOT, body, asset_name, asset_name), palette
            )

unreal.log("TACTICAL_LOOK complete: 8 team material instances updated")
