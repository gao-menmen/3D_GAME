import unreal
bp = unreal.load_asset('/ShooterCore/Bot/B_ShooterBotSpawner')
gc = bp.generated_class()
cdo = unreal.get_default_object(gc)
print('PROBE spawner cdo:', cdo.get_class().get_name())
try:
    print('PROBE NumBots:', cdo.get_editor_property('NumBotsToCreate'))
except Exception as e:
    print('PROBE NumBots FAIL:', repr(e))
try:
    print('PROBE BotControllerClass:', cdo.get_editor_property('BotControllerClass'))
except Exception as e:
    print('PROBE BotControllerClass FAIL:', repr(e))
