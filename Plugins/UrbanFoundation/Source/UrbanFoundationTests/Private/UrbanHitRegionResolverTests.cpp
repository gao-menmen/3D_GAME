#include "Combat/UrbanHitRegionResolver.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanHitRegionBoneMappingTest,
    "UrbanSpear.Combat.Damage.BoneMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanHitRegionBoneMappingTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    TestEqual(TEXT("head bone maps to head"), UUrbanHitRegionResolver::ResolveBoneName(TEXT("head")), EUrbanHitRegion::Head);
    TestEqual(TEXT("facial child maps to head"), UUrbanHitRegionResolver::ResolveBoneName(TEXT("head_face")), EUrbanHitRegion::Head);
    TestEqual(TEXT("spine maps to torso"), UUrbanHitRegionResolver::ResolveBoneName(TEXT("spine_03")), EUrbanHitRegion::Torso);
    TestEqual(TEXT("pelvis maps to torso"), UUrbanHitRegionResolver::ResolveBoneName(TEXT("pelvis")), EUrbanHitRegion::Torso);
    TestEqual(TEXT("neck maps to protected torso"), UUrbanHitRegionResolver::ResolveBoneName(TEXT("neck_01")), EUrbanHitRegion::Torso);
    TestEqual(TEXT("arm maps to limb"), UUrbanHitRegionResolver::ResolveBoneName(TEXT("upperarm_l")), EUrbanHitRegion::Limb);
    TestEqual(TEXT("calf maps to limb"), UUrbanHitRegionResolver::ResolveBoneName(TEXT("calf_r")), EUrbanHitRegion::Limb);
    TestEqual(TEXT("missing bone safely defaults to torso"), UUrbanHitRegionResolver::ResolveBoneName(NAME_None), EUrbanHitRegion::Torso);
    return true;
}

#endif
