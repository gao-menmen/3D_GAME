#include "Combat/UrbanCombatTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanAmmoStateLifecycleTest,
    "UrbanSpear.Combat.Ammo.StateLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanAmmoStateLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanAmmoState Ammo;
    Ammo.Initialize(30, 2, 10);
    TestTrue(TEXT("initialized ammo can fire"), Ammo.CanFire());
    TestTrue(TEXT("first round is consumed"), Ammo.TryConsumeRound());
    TestEqual(TEXT("one round remains"), Ammo.AmmoInMagazine, 1);
    TestTrue(TEXT("second round is consumed"), Ammo.TryConsumeRound());
    TestEqual(TEXT("magazine becomes empty"), Ammo.AmmoInMagazine, 0);
    TestFalse(TEXT("empty magazine cannot fire"), Ammo.TryConsumeRound());

    TestTrue(TEXT("empty magazine can begin reload"), Ammo.BeginReload());
    TestFalse(TEXT("reloading blocks fire"), Ammo.CanFire());
    TestFalse(TEXT("reload cannot begin twice"), Ammo.BeginReload());
    TestEqual(TEXT("reload transfers available reserve"), Ammo.CompleteReload(), 10);
    TestEqual(TEXT("magazine receives reserve"), Ammo.AmmoInMagazine, 10);
    TestEqual(TEXT("reserve becomes empty"), Ammo.ReserveAmmo, 0);
    TestFalse(TEXT("cannot reload with no reserve"), Ammo.BeginReload());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanAmmoStatePartialReloadTest,
    "UrbanSpear.Combat.Ammo.PartialReloadAndCancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanAmmoStatePartialReloadTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanAmmoState Ammo;
    Ammo.Initialize(30, 12, 100);
    TestTrue(TEXT("partial magazine begins reload"), Ammo.BeginReload());
    Ammo.CancelReload();
    TestFalse(TEXT("cancel ends reload"), Ammo.bReloading);
    TestEqual(TEXT("cancel preserves magazine"), Ammo.AmmoInMagazine, 12);
    TestEqual(TEXT("cancel preserves reserve"), Ammo.ReserveAmmo, 100);

    TestTrue(TEXT("reload can restart"), Ammo.BeginReload());
    TestEqual(TEXT("full reload transfers missing rounds"), Ammo.CompleteReload(), 18);
    TestEqual(TEXT("magazine is full"), Ammo.AmmoInMagazine, 30);
    TestEqual(TEXT("reserve is reduced"), Ammo.ReserveAmmo, 82);
    TestFalse(TEXT("full magazine cannot reload"), Ammo.BeginReload());

    Ammo.Initialize(-5, 999, -1);
    TestEqual(TEXT("capacity sanitizes"), Ammo.MagazineCapacity, 1);
    TestEqual(TEXT("magazine sanitizes to capacity"), Ammo.AmmoInMagazine, 1);
    TestEqual(TEXT("reserve sanitizes to zero"), Ammo.ReserveAmmo, 0);

    return true;
}

#endif
