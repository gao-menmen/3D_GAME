#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/UrbanCombatTypes.h"
#include "UrbanCombatCatalog.generated.h"

UCLASS()
class URBANCOMBAT_API UUrbanCombatCatalog : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat")
    static FUrbanWeaponTuning GetWeaponDefaults(EUrbanWeaponArchetype Archetype);

    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat")
    static TArray<FUrbanWeaponTuning> GetAllWeaponDefaults();

    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat")
    static FUrbanTacticalItemTuning GetTacticalItemDefaults(EUrbanTacticalItemType ItemType);

    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat")
    static TArray<FUrbanTacticalItemTuning> GetAllTacticalItemDefaults();
};
