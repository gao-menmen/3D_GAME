#include "Camera/LyraCameraMode_UrbanPerspective.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCameraTransitionCollisionTest,
    "UrbanSpear.CharacterCamera.CameraMode.TransitionFramesApplyCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCameraTransitionCollisionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FVector TransitionStartLocation(-300.0f, -100.0f, 80.0f);
    const FVector DesiredLocation(-300.0f, 100.0f, 80.0f);
    const FVector ExpectedInterpolatedLocation(-300.0f, 0.0f, 80.0f);
    const FVector CollisionConstrainedLocation(-300.0f, -25.0f, 80.0f);

    int32 ConstraintCallCount = 0;
    FVector ConstraintInput = FVector::ZeroVector;
    const FVector ResolvedLocation = UrbanCameraTransition::ResolveFrameLocation(
        TransitionStartLocation,
        DesiredLocation,
        0.5f,
        [&ConstraintCallCount, &ConstraintInput, &CollisionConstrainedLocation](
            const FVector& InterpolatedLocation)
        {
            ++ConstraintCallCount;
            ConstraintInput = InterpolatedLocation;
            return CollisionConstrainedLocation;
        });

    TestEqual(
        TEXT("collision is evaluated once for the transition frame"),
        ConstraintCallCount,
        1);
    TestEqual(
        TEXT("collision receives the interpolated frame location rather than only the safe target"),
        ConstraintInput,
        ExpectedInterpolatedLocation);
    TestEqual(
        TEXT("transition frame uses the collision-constrained location"),
        ResolvedLocation,
        CollisionConstrainedLocation);

    return true;
}

#endif