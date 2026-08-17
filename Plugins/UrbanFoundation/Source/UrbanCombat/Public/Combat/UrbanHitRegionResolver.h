#pragma once

#include "CoreMinimal.h"
#include "Combat/UrbanCombatTypes.h"
#include "UrbanHitRegionResolver.generated.h"

/** Maps skeletal hit bones into deterministic tactical damage regions. */
UCLASS()
class URBANCOMBAT_API UUrbanHitRegionResolver : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat")
    static EUrbanHitRegion ResolveBoneName(FName BoneName);
};
