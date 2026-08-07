#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/UrbanCombatTypes.h"
#include "UrbanDamageModel.generated.h"

UCLASS()
class URBANCOMBAT_API UUrbanDamageModel : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat|Damage")
    static FUrbanDamageResult CalculateDamage(
        float CurrentHealth,
        float CurrentArmor,
        const FUrbanDamageProfile& DamageProfile,
        const FUrbanDamageRequest& DamageRequest);
};
