#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/UrbanCombatTypes.h"
#include "UrbanCombatDefinitions.generated.h"

UCLASS(BlueprintType)
class URBANCOMBAT_API UUrbanWeaponDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|Combat")
    FUrbanWeaponTuning Tuning;

    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat")
    FUrbanWeaponTuning GetSanitizedTuning() const;
};

UCLASS(BlueprintType)
class URBANCOMBAT_API UUrbanTacticalItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|Combat")
    FUrbanTacticalItemTuning Tuning;

    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat")
    FUrbanTacticalItemTuning GetSanitizedTuning() const;
};
