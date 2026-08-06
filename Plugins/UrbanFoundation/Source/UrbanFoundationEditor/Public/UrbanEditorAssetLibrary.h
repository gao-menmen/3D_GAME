#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UrbanEditorAssetLibrary.generated.h"

class AWorldSettings;

UCLASS()
class URBANFOUNDATIONEDITOR_API UUrbanEditorAssetLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Urban Spear|Editor")
    static bool SetDefaultGameplayExperience(
        AWorldSettings* WorldSettings,
        const FString& ExperienceClassPath);
};
