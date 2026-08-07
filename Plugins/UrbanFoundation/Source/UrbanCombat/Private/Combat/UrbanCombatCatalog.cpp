#include "Combat/UrbanCombatCatalog.h"

FUrbanWeaponTuning UUrbanCombatCatalog::GetWeaponDefaults(const EUrbanWeaponArchetype Archetype)
{
    return FUrbanWeaponTuning::MakeDefaults(Archetype);
}

TArray<FUrbanWeaponTuning> UUrbanCombatCatalog::GetAllWeaponDefaults()
{
    return {
        FUrbanWeaponTuning::MakeDefaults(EUrbanWeaponArchetype::AssaultRifle),
        FUrbanWeaponTuning::MakeDefaults(EUrbanWeaponArchetype::SubmachineGun),
        FUrbanWeaponTuning::MakeDefaults(EUrbanWeaponArchetype::TacticalPistol),
    };
}

FUrbanTacticalItemTuning UUrbanCombatCatalog::GetTacticalItemDefaults(const EUrbanTacticalItemType ItemType)
{
    return FUrbanTacticalItemTuning::MakeDefaults(ItemType);
}

TArray<FUrbanTacticalItemTuning> UUrbanCombatCatalog::GetAllTacticalItemDefaults()
{
    return {
        FUrbanTacticalItemTuning::MakeDefaults(EUrbanTacticalItemType::FragGrenade),
        FUrbanTacticalItemTuning::MakeDefaults(EUrbanTacticalItemType::SmokeGrenade),
        FUrbanTacticalItemTuning::MakeDefaults(EUrbanTacticalItemType::Medkit),
        FUrbanTacticalItemTuning::MakeDefaults(EUrbanTacticalItemType::ReconDrone),
    };
}
