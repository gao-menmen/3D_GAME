#include "Engine/AssetManagerSettings.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanExperiencePrimaryAssetRegistrationTest,
    "UrbanSpear.CharacterCamera.Experience.PrimaryAssetRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanExperiencePrimaryAssetRegistrationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const UAssetManagerSettings* Settings = GetDefault<UAssetManagerSettings>();
    TestNotNull(TEXT("asset manager settings are available"), Settings);
    if (Settings == nullptr)
    {
        return false;
    }

    const FName ExperienceType(TEXT("LyraExperienceDefinition"));
    const FString ExperienceDirectory(TEXT("/UrbanFoundation/Experiences"));
    const FSoftObjectPath ExperienceAsset(
        TEXT("/UrbanFoundation/Experiences/B_UrbanCharacterCameraExperience.B_UrbanCharacterCameraExperience"));

    const bool bExperienceIsRegistered = Settings->PrimaryAssetTypesToScan.ContainsByPredicate(
        [&ExperienceType, &ExperienceDirectory, &ExperienceAsset](const FPrimaryAssetTypeInfo& TypeInfo)
        {
            if (TypeInfo.PrimaryAssetType != ExperienceType)
            {
                return false;
            }

            const bool bDirectoryIsScanned = TypeInfo.GetDirectories().ContainsByPredicate(
                [&ExperienceDirectory](const FDirectoryPath& Directory)
                {
                    return Directory.Path.Equals(ExperienceDirectory, ESearchCase::CaseSensitive);
                });
            const bool bAssetIsExplicitlyScanned = TypeInfo.GetSpecificAssets().Contains(ExperienceAsset);
            return bDirectoryIsScanned || bAssetIsExplicitlyScanned;
        });

    TestTrue(
        TEXT("the packaged runtime scans the Urban character-camera experience as a LyraExperienceDefinition"),
        bExperienceIsRegistered);

    return true;
}

#endif
