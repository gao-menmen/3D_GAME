import unreal
bp = unreal.load_asset('/ShooterCore/Game/B_TeamSetup_TwoTeams')
print('PROBE bp:', bp)
gc = bp.generated_class()
cdo = unreal.get_default_object(gc)
print('PROBE cdo:', cdo.get_class().get_name())
try:
    tt = cdo.get_editor_property('TeamsToCreate')
    print('PROBE TeamsToCreate:', tt)
    print('PROBE type:', type(tt))
except Exception as e:
    print('PROBE TeamsToCreate FAIL:', repr(e))
try:
    import unreal as u
    # 尝试按键遍历
    print('PROBE has TeamsToCreate prop:', 'TeamsToCreate' in [p for p in dir(cdo)])
except Exception as e:
    print('PROBE FAIL2:', repr(e))
