"""
Diag-Enum.py - find the correct Python name for the EUrbanViewPolicy enum.
"""

import unreal


def main():
    candidates = [
        "EUrbanViewPolicy",
        "UrbanViewPolicy",
        "EUrbanPerspective",
        "UrbanPerspective",
    ]
    for name in candidates:
        attr = getattr(unreal, name, None)
        if attr is not None:
            unreal.log(f"FOUND: unreal.{name} = {attr}")
            if hasattr(attr, "FIRST_PERSON_ONLY"):
                unreal.log(f"  FIRST_PERSON_ONLY value = {attr.FIRST_PERSON_ONLY}")
            if hasattr(attr, "FREE_CHOICE"):
                unreal.log(f"  FREE_CHOICE value = {attr.FREE_CHOICE}")
        else:
            unreal.log(f"missing: unreal.{name}")

    # Fallback: scan the module for anything containing 'ViewPolicy' or 'Perspective'
    hits = [n for n in dir(unreal) if "ViewPolicy" in n or "Perspective" in n or "UrbanView" in n]
    unreal.log(f"Module names containing ViewPolicy/Perspective/UrbanView: {hits}")


main()
