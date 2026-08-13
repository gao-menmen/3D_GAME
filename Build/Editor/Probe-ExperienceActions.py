import unreal

print('PROBE start')

# 1. 能否 load_class 拿到 AddComponents 类
try:
    cls = unreal.load_class(None, '/Script/GameFeatures.GameFeatureAction_AddComponents')
    print('PROBE load_class AddComponents:', cls)
except Exception as e:
    print('PROBE load_class FAIL:', repr(e))

# 2. 能否 new_object 创建实例
try:
    bp = unreal.load_asset('/UrbanFoundation/Experiences/B_UrbanCharacterCameraExperience')
    gc = bp.generated_class()
    cdo = unreal.get_default_object(gc)
    obj = unreal.new_object(cls, cdo, 'GameFeatureAction_AddComponents_Bots')
    print('PROBE new_object:', obj)
except Exception as e:
    print('PROBE new_object FAIL:', repr(e))

# 3. 能否拿到 ComponentEntry struct 并实例化
try:
    st = unreal.load_struct('/Script/GameFeatures.GameFeatureComponentEntry')
    print('PROBE load_struct ComponentEntry:', st)
except Exception as e:
    print('PROBE load_struct FAIL:', repr(e))

# 4. struct 实例化尝试
try:
    entry = unreal.GameFeatureComponentEntry()
    print('PROBE direct struct:', entry)
except Exception as e:
    print('PROBE direct struct FAIL:', repr(e))

# 5. 读 Elimination Experience 的 AddComponents action（看它有没有 component_list 可读）
try:
    bp2 = unreal.load_asset('/ShooterCore/Experiences/B_ShooterGame_Elimination')
    gc2 = bp2.generated_class()
    cdo2 = unreal.get_default_object(gc2)
    actions2 = cdo2.get_editor_property('actions')
    print('PROBE Elimination actions:', len(actions2))
    for a in actions2:
        print('PROBE  Elimination action:', a.get_class().get_name())
except Exception as e:
    print('PROBE Elimination read FAIL:', repr(e))
