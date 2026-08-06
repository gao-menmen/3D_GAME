#include "UrbanEditorAssetLibrary.h"

#include "GameFramework/WorldSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

bool UUrbanEditorAssetLibrary::SetDefaultGameplayExperience(
    AWorldSettings* WorldSettings,
    const FString& ExperienceClassPath)
{
    if (!IsValid(WorldSettings))
    {
        return false;
    }

    FSoftClassProperty* ExperienceProperty = FindFProperty<FSoftClassProperty>(
        WorldSettings->GetClass(),
        TEXT("DefaultGameplayExperience"));
    const FSoftObjectPath ExperiencePath(ExperienceClassPath);
    if (!ExperienceProperty || !ExperiencePath.IsValid())
    {
        return false;
    }

    WorldSettings->Modify();
    ExperienceProperty->SetPropertyValue_InContainer(
        WorldSettings,
        FSoftObjectPtr(ExperiencePath));

    FPropertyChangedEvent ChangeEvent(ExperienceProperty, EPropertyChangeType::ValueSet);
    WorldSettings->PostEditChangeProperty(ChangeEvent);
    WorldSettings->MarkPackageDirty();
    return true;
}
