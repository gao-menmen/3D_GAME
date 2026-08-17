#include "Camera/LyraCameraMode_UrbanPerspective.h"
#include "Camera/UrbanCameraSettings.h"
#include "Character/UrbanPerspectiveTypes.h"
#include "Misc/AutomationTest.h"
#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCameraSettingsDefaultsTest,
    "UrbanSpear.CharacterCamera.CameraSettings.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCameraSettingsDefaultsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FUrbanCameraSettings Settings;
    TestEqual(TEXT("first person FOV defaults to 90"), Settings.FirstPersonFieldOfView, 90.0f);
    TestEqual(TEXT("third person FOV defaults to 85"), Settings.ThirdPersonFieldOfView, 85.0f);
    TestEqual(TEXT("right shoulder offset defaults to 55"), Settings.RightShoulderOffsetY, 55.0f);
    TestEqual(TEXT("left shoulder offset defaults to -55"), Settings.LeftShoulderOffsetY, -55.0f);
    TestEqual(TEXT("perspective transition defaults to 0.2 seconds"), Settings.TransitionTime, 0.2f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCameraSettingsSanitizeTest,
    "UrbanSpear.CharacterCamera.CameraSettings.Sanitize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCameraSettingsSanitizeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanCameraSettings Minimums;
    Minimums.FirstPersonFieldOfView = -1.0f;
    Minimums.ThirdPersonFieldOfView = 1.0f;
    Minimums.ThirdPersonDistance = -100.0f;
    Minimums.CollisionRadius = 0.0f;
    Minimums.TransitionTime = 0.0f;
    Minimums.Sanitize();

    TestEqual(TEXT("first person FOV clamps to minimum"), Minimums.FirstPersonFieldOfView, 60.0f);
    TestEqual(TEXT("third person FOV clamps to minimum"), Minimums.ThirdPersonFieldOfView, 60.0f);
    TestEqual(TEXT("third person distance clamps to minimum"), Minimums.ThirdPersonDistance, 80.0f);
    TestEqual(TEXT("collision radius clamps to minimum"), Minimums.CollisionRadius, 4.0f);
    TestEqual(TEXT("transition time clamps to minimum"), Minimums.TransitionTime, 0.15f);

    FUrbanCameraSettings Maximums;
    Maximums.FirstPersonFieldOfView = 999.0f;
    Maximums.ThirdPersonFieldOfView = 999.0f;
    Maximums.ThirdPersonDistance = 999.0f;
    Maximums.CollisionRadius = 999.0f;
    Maximums.TransitionTime = 999.0f;
    Maximums.Sanitize();

    TestEqual(TEXT("first person FOV clamps to maximum"), Maximums.FirstPersonFieldOfView, 120.0f);
    TestEqual(TEXT("third person FOV clamps to maximum"), Maximums.ThirdPersonFieldOfView, 120.0f);
    TestEqual(TEXT("third person distance clamps to maximum"), Maximums.ThirdPersonDistance, 450.0f);
    TestEqual(TEXT("collision radius clamps to maximum"), Maximums.CollisionRadius, 30.0f);
    TestEqual(TEXT("transition time clamps to maximum"), Maximums.TransitionTime, 0.30f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectiveCameraModeContractTest,
    "UrbanSpear.CharacterCamera.CameraMode.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectiveCameraModeContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    static_assert(
        std::is_base_of_v<ULyraCameraMode, ULyraCameraMode_UrbanPerspective>,
        "The Urban perspective camera must remain compatible with the Lyra camera stack.");

    const ULyraCameraMode_UrbanPerspective* CameraMode = GetDefault<ULyraCameraMode_UrbanPerspective>();
    TestNotNull(TEXT("camera mode has a class default object"), CameraMode);
    if (CameraMode == nullptr)
    {
        return false;
    }

    TestFalse(
        TEXT("first-person uses the first-person camera path"),
        ULyraCameraMode_UrbanPerspective::IsThirdPerson(EUrbanPerspective::FirstPerson));
    TestTrue(
        TEXT("right shoulder uses the third-person camera path"),
        ULyraCameraMode_UrbanPerspective::IsThirdPerson(EUrbanPerspective::ThirdPersonRight));
    TestTrue(
        TEXT("left shoulder uses the third-person camera path"),
        ULyraCameraMode_UrbanPerspective::IsThirdPerson(EUrbanPerspective::ThirdPersonLeft));
    const FUrbanCameraSettings& Settings = CameraMode->GetCameraSettings();
    TestTrue(TEXT("camera mode exposes sanitized settings"), Settings.IsWithinSafeRanges());
    TestEqual(
        TEXT("right shoulder offset uses positive configured Y"),
        Settings.GetThirdPersonOffset(EUrbanPerspective::ThirdPersonRight),
        FVector(-280.0f, 55.0f, 20.0f));
    TestEqual(
        TEXT("left shoulder offset uses negative configured Y"),
        Settings.GetThirdPersonOffset(EUrbanPerspective::ThirdPersonLeft),
        FVector(-280.0f, -55.0f, 20.0f));

    return true;
}

#endif
