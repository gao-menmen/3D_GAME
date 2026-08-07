"""
Diag-Enum2.py - enumerate members of unreal.UrbanViewPolicy.
"""

import unreal


def main():
    cls = unreal.UrbanViewPolicy
    unreal.log(f"Members of unreal.UrbanViewPolicy:")
    for name in dir(cls):
        if name.startswith("_"):
            continue
        try:
            val = getattr(cls, name)
            unreal.log(f"  {name} = {val}")
        except Exception as e:
            unreal.log(f"  {name} -> {e}")


main()
