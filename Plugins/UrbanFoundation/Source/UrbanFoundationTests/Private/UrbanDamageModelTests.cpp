#include "Combat/UrbanDamageModel.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanDamageRegionMultiplierTest,
    "UrbanSpear.Combat.Damage.HitRegions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanDamageRegionMultiplierTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FUrbanDamageProfile Profile;

    FUrbanDamageRequest HeadRequest;
    HeadRequest.RawDamage = 40.0f;
    HeadRequest.HitRegion = EUrbanHitRegion::Head;
    const FUrbanDamageResult HeadResult = UUrbanDamageModel::CalculateDamage(100.0f, 100.0f, Profile, HeadRequest);
    TestEqual(TEXT("head applies double damage"), HeadResult.AppliedHealthDamage, 80.0f, 0.001f);
    TestEqual(TEXT("head bypasses torso armor"), HeadResult.AppliedArmorDamage, 0.0f, 0.001f);

    FUrbanDamageRequest LimbRequest;
    LimbRequest.RawDamage = 40.0f;
    LimbRequest.HitRegion = EUrbanHitRegion::Limb;
    const FUrbanDamageResult LimbResult = UUrbanDamageModel::CalculateDamage(100.0f, 100.0f, Profile, LimbRequest);
    TestEqual(TEXT("limb applies reduced damage"), LimbResult.AppliedHealthDamage, 28.0f, 0.001f);
    TestEqual(TEXT("limb bypasses torso armor"), LimbResult.AppliedArmorDamage, 0.0f, 0.001f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanDamageArmorAbsorptionTest,
    "UrbanSpear.Combat.Damage.TorsoArmor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanDamageArmorAbsorptionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FUrbanDamageProfile Profile;
    FUrbanDamageRequest Request;
    Request.RawDamage = 40.0f;
    Request.HitRegion = EUrbanHitRegion::Torso;

    const FUrbanDamageResult Result = UUrbanDamageModel::CalculateDamage(100.0f, 100.0f, Profile, Request);
    TestEqual(TEXT("torso armor absorbs 65 percent"), Result.AppliedArmorDamage, 26.0f, 0.001f);
    TestEqual(TEXT("remaining torso damage reaches health"), Result.AppliedHealthDamage, 14.0f, 0.001f);
    TestEqual(TEXT("armor durability is reduced"), Result.RemainingArmor, 74.0f, 0.001f);
    TestEqual(TEXT("health is reduced"), Result.RemainingHealth, 86.0f, 0.001f);
    TestFalse(TEXT("non-lethal torso hit does not kill"), Result.bKilled);

    const FUrbanDamageResult BrokenArmorResult = UUrbanDamageModel::CalculateDamage(100.0f, 5.0f, Profile, Request);
    TestEqual(TEXT("armor damage is capped by remaining armor"), BrokenArmorResult.AppliedArmorDamage, 5.0f, 0.001f);
    TestEqual(TEXT("unabsorbed damage reaches health"), BrokenArmorResult.AppliedHealthDamage, 35.0f, 0.001f);
    TestEqual(TEXT("armor reaches zero"), BrokenArmorResult.RemainingArmor, 0.0f, 0.001f);

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanDamageHelmetArmorTest,
    "UrbanSpear.Combat.Damage.HelmetArmor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanDamageHelmetArmorTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FUrbanDamageProfile Profile;
    FUrbanDamageRequest Request;
    Request.RawDamage = 40.0f;
    Request.HitRegion = EUrbanHitRegion::Head;

    const FUrbanDamageResult Unprotected = UUrbanDamageModel::CalculateDamage(100.0f, 100.0f, Profile, Request);
    TestEqual(TEXT("headshots bypass armor without a helmet"), Unprotected.AppliedHealthDamage, 80.0f, 0.001f);
    TestEqual(TEXT("unprotected headshot preserves armor"), Unprotected.AppliedArmorDamage, 0.0f, 0.001f);

    Request.bHasHelmet = true;
    const FUrbanDamageResult Protected = UUrbanDamageModel::CalculateDamage(100.0f, 100.0f, Profile, Request);
    TestEqual(TEXT("helmet absorbs half of scaled headshot damage"), Protected.AppliedArmorDamage, 40.0f, 0.001f);
    TestEqual(TEXT("helmet lets remaining headshot damage reach health"), Protected.AppliedHealthDamage, 40.0f, 0.001f);

    const FUrbanDamageResult BrokenHelmet = UUrbanDamageModel::CalculateDamage(100.0f, 10.0f, Profile, Request);
    TestEqual(TEXT("helmet absorption is capped by durability"), BrokenHelmet.AppliedArmorDamage, 10.0f, 0.001f);
    TestEqual(TEXT("damage beyond helmet durability reaches health"), BrokenHelmet.AppliedHealthDamage, 70.0f, 0.001f);

    Request.HitRegion = EUrbanHitRegion::Limb;
    const FUrbanDamageResult Limb = UUrbanDamageModel::CalculateDamage(100.0f, 100.0f, Profile, Request);
    TestEqual(TEXT("helmet does not protect limbs"), Limb.AppliedHealthDamage, 28.0f, 0.001f);
    TestEqual(TEXT("limb hit preserves armor"), Limb.AppliedArmorDamage, 0.0f, 0.001f);

    Request.HitRegion = EUrbanHitRegion::Head;
    const FUrbanDamageResult NoDurability = UUrbanDamageModel::CalculateDamage(100.0f, 0.0f, Profile, Request);
    TestEqual(TEXT("helmet needs armor durability to absorb"), NoDurability.AppliedHealthDamage, 80.0f, 0.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanDamageLethalClampTest,
    "UrbanSpear.Combat.Damage.LethalClamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanDamageLethalClampTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanDamageProfile Profile;
    FUrbanDamageRequest Request;
    Request.RawDamage = 1000.0f;
    Request.HitRegion = EUrbanHitRegion::Head;

    const FUrbanDamageResult Result = UUrbanDamageModel::CalculateDamage(25.0f, 0.0f, Profile, Request);
    TestEqual(TEXT("applied health damage is capped by current health"), Result.AppliedHealthDamage, 25.0f, 0.001f);
    TestEqual(TEXT("health never becomes negative"), Result.RemainingHealth, 0.0f, 0.001f);
    TestTrue(TEXT("zero remaining health is lethal"), Result.bKilled);

    Request.RawDamage = -10.0f;
    const FUrbanDamageResult NegativeResult = UUrbanDamageModel::CalculateDamage(25.0f, 10.0f, Profile, Request);
    TestEqual(TEXT("negative damage does not change health"), NegativeResult.RemainingHealth, 25.0f, 0.001f);
    TestEqual(TEXT("negative damage does not change armor"), NegativeResult.RemainingArmor, 10.0f, 0.001f);
    TestFalse(TEXT("negative damage is not lethal"), NegativeResult.bKilled);

    return true;
}

#endif
