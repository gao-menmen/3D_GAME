#include "Combat/UrbanCombatDefinitions.h"

FUrbanWeaponTuning UUrbanWeaponDefinition::GetSanitizedTuning() const
{
    FUrbanWeaponTuning Result = Tuning;
    Result.Sanitize();
    return Result;
}

FUrbanTacticalItemTuning UUrbanTacticalItemDefinition::GetSanitizedTuning() const
{
    FUrbanTacticalItemTuning Result = Tuning;
    Result.Sanitize();
    return Result;
}
