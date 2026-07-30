#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanModuleLoadTest,
    "UrbanSpear.Foundation.ModuleLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanModuleLoadTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TCHAR* Names[] =
    {
        TEXT("UrbanCore"),
        TEXT("UrbanCombat"),
        TEXT("UrbanAI"),
        TEXT("UrbanMission"),
        TEXT("UrbanModes"),
        TEXT("UrbanUI"),
        TEXT("UrbanOnline"),
    };

    for (const TCHAR* Name : Names)
    {
        TestTrue(FString::Printf(TEXT("%s registered"), Name), FModuleManager::Get().ModuleExists(Name));
        TestTrue(
            FString::Printf(TEXT("%s loads"), Name),
            FModuleManager::Get().LoadModulePtr<IModuleInterface>(Name) != nullptr);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanProjectIdentityTest,
    "UrbanSpear.Foundation.ProjectIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanProjectIdentityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    TestEqual(TEXT("Project name"), FString(FApp::GetProjectName()), FString(TEXT("UrbanSpear")));
    TestTrue(
        TEXT("Lyra front-end map exists"),
        FPackageName::DoesPackageExist(TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd")));

    return true;
}

#endif