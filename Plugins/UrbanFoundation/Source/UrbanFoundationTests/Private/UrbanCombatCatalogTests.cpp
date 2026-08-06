#include "Combat/UrbanCombatCatalog.h"
#include "Combat/UrbanCombatDefinitions.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCombatWeaponCatalogTest,
    "UrbanSpear.Combat.Catalog.ThreeWeaponArchetypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCombatWeaponCatalogTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TArray<FUrbanWeaponTuning> Weapons = UUrbanCombatCatalog::GetAllWeaponDefaults();
    TestEqual(TEXT("combat catalog contains exactly three weapons"), Weapons.Num(), 3);

    TSet<EUrbanWeaponArchetype> Archetypes;
    TSet<FName> WeaponIds;
    for (const FUrbanWeaponTuning& Weapon : Weapons)
    {
        Archetypes.Add(Weapon.Archetype);
        WeaponIds.Add(Weapon.WeaponId);
        TestTrue(TEXT("weapon magazine is positive"), Weapon.MagazineCapacity > 0);
        TestTrue(TEXT("weapon reserve is non-negative"), Weapon.StartingReserveAmmo >= 0);
        TestTrue(TEXT("weapon damage is positive"), Weapon.BaseDamage > 0.0f);
        TestTrue(TEXT("weapon supports its preferred fire mode"), Weapon.SupportsFireMode(Weapon.PreferredFireMode));
        TestTrue(TEXT("aim spread is no larger than hip spread"), Weapon.AimSpreadDegrees <= Weapon.HipFireSpreadDegrees);
    }

    TestEqual(TEXT("all weapon archetypes are unique"), Archetypes.Num(), 3);
    TestEqual(TEXT("all weapon ids are unique"), WeaponIds.Num(), 3);

    const FUrbanWeaponTuning Rifle = UUrbanCombatCatalog::GetWeaponDefaults(EUrbanWeaponArchetype::AssaultRifle);
    TestTrue(TEXT("rifle supports semi automatic"), Rifle.SupportsFireMode(EUrbanFireMode::SemiAutomatic));
    TestTrue(TEXT("rifle supports automatic"), Rifle.SupportsFireMode(EUrbanFireMode::Automatic));

    const FUrbanWeaponTuning Pistol = UUrbanCombatCatalog::GetWeaponDefaults(EUrbanWeaponArchetype::TacticalPistol);
    TestTrue(TEXT("pistol supports semi automatic"), Pistol.SupportsFireMode(EUrbanFireMode::SemiAutomatic));
    TestFalse(TEXT("pistol does not support automatic"), Pistol.SupportsFireMode(EUrbanFireMode::Automatic));
    TestEqual(TEXT("pistol magazine defaults to 15"), Pistol.MagazineCapacity, 15);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCombatWeaponSanitizeTest,
    "UrbanSpear.Combat.Catalog.WeaponSanitize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCombatWeaponSanitizeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanWeaponTuning Tuning;
    Tuning.WeaponId = NAME_None;
    Tuning.MagazineCapacity = -10;
    Tuning.StartingReserveAmmo = -20;
    Tuning.RoundsPerMinute = 0.0f;
    Tuning.BaseDamage = -1.0f;
    Tuning.HipFireSpreadDegrees = 1.0f;
    Tuning.AimSpreadDegrees = 9.0f;
    Tuning.bSupportsSemiAutomatic = false;
    Tuning.bSupportsAutomatic = false;
    Tuning.PreferredFireMode = EUrbanFireMode::Automatic;
    Tuning.Sanitize();

    TestFalse(TEXT("sanitized weapon id is not none"), Tuning.WeaponId.IsNone());
    TestEqual(TEXT("magazine clamps to one"), Tuning.MagazineCapacity, 1);
    TestEqual(TEXT("reserve clamps to zero"), Tuning.StartingReserveAmmo, 0);
    TestEqual(TEXT("rate of fire clamps to minimum"), Tuning.RoundsPerMinute, 60.0f);
    TestEqual(TEXT("damage clamps to minimum"), Tuning.BaseDamage, 1.0f);
    TestEqual(TEXT("aim spread clamps to hip spread"), Tuning.AimSpreadDegrees, 1.0f);
    TestTrue(TEXT("at least semi automatic is restored"), Tuning.bSupportsSemiAutomatic);
    TestEqual(TEXT("unsupported preferred mode is corrected"), Tuning.PreferredFireMode, EUrbanFireMode::SemiAutomatic);

    UUrbanWeaponDefinition* Definition = NewObject<UUrbanWeaponDefinition>();
    Definition->Tuning = Tuning;
    Definition->Tuning.MagazineCapacity = 9999;
    const FUrbanWeaponTuning SafeDefinition = Definition->GetSanitizedTuning();
    TestEqual(TEXT("data asset returns sanitized tuning"), SafeDefinition.MagazineCapacity, 500);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCombatTacticalCatalogTest,
    "UrbanSpear.Combat.Catalog.FourTacticalItems",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCombatTacticalCatalogTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TArray<FUrbanTacticalItemTuning> Items = UUrbanCombatCatalog::GetAllTacticalItemDefaults();
    TestEqual(TEXT("catalog contains exactly four tactical items"), Items.Num(), 4);

    TSet<EUrbanTacticalItemType> ItemTypes;
    TSet<FName> ItemIds;
    for (const FUrbanTacticalItemTuning& Item : Items)
    {
        ItemTypes.Add(Item.ItemType);
        ItemIds.Add(Item.ItemId);
        TestTrue(TEXT("tactical item has at least one charge"), Item.MaximumCharges > 0);
        TestFalse(TEXT("tactical item id is not none"), Item.ItemId.IsNone());
    }

    TestEqual(TEXT("all tactical item types are unique"), ItemTypes.Num(), 4);
    TestEqual(TEXT("all tactical item ids are unique"), ItemIds.Num(), 4);

    const FUrbanTacticalItemTuning Drone = UUrbanCombatCatalog::GetTacticalItemDefaults(EUrbanTacticalItemType::ReconDrone);
    TestTrue(TEXT("recon drone is range limited"), Drone.MaximumUseRangeMeters > 0.0f);
    TestTrue(TEXT("recon drone is time limited"), Drone.EffectDurationSeconds > 0.0f);

    return true;
}

#endif
