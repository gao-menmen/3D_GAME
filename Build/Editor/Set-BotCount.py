import unreal

# Tune the bot count for the arena: save the change to the asset.
BP_PATH = "/ShooterCore/Bot/B_ShooterBotSpawner"
TARGET_COUNT = 8

bp = unreal.load_asset(BP_PATH)
if not bp:
    raise RuntimeError(f"Unable to load {BP_PATH}")

gc = bp.generated_class()
cdo = unreal.get_default_object(gc)
old = cdo.get_editor_property("NumBotsToCreate")
cdo.set_editor_property("NumBotsToCreate", TARGET_COUNT)
print(f"BOTCOUNT: {BP_PATH} NumBotsToCreate {old} -> {TARGET_COUNT}")

saved = unreal.EditorAssetLibrary.save_asset(BP_PATH)
print(f"BOTCOUNT: save_asset result = {saved}")
