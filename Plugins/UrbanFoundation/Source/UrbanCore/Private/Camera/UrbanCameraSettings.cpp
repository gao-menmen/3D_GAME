#include "Camera/UrbanCameraSettings.h"

FUrbanCameraSettings::FUrbanCameraSettings()
    : FirstPersonFieldOfView(90.0f)
    , ThirdPersonFieldOfView(85.0f)
    , ThirdPersonDistance(280.0f)
    , RightShoulderOffsetY(55.0f)
    , LeftShoulderOffsetY(-55.0f)
    , ThirdPersonVerticalOffset(20.0f)
    , CollisionRadius(14.0f)
    , TransitionTime(0.2f)
{
}

void FUrbanCameraSettings::Sanitize()
{
    FirstPersonFieldOfView = FMath::Clamp(FirstPersonFieldOfView, 60.0f, 120.0f);
    ThirdPersonFieldOfView = FMath::Clamp(ThirdPersonFieldOfView, 60.0f, 120.0f);
    ThirdPersonDistance = FMath::Clamp(ThirdPersonDistance, 80.0f, 450.0f);
    CollisionRadius = FMath::Clamp(CollisionRadius, 4.0f, 30.0f);
    TransitionTime = FMath::Clamp(TransitionTime, 0.15f, 0.30f);
}

bool FUrbanCameraSettings::IsWithinSafeRanges() const
{
    return FMath::IsWithinInclusive(FirstPersonFieldOfView, 60.0f, 120.0f)
        && FMath::IsWithinInclusive(ThirdPersonFieldOfView, 60.0f, 120.0f)
        && FMath::IsWithinInclusive(ThirdPersonDistance, 80.0f, 450.0f)
        && FMath::IsWithinInclusive(CollisionRadius, 4.0f, 30.0f)
        && FMath::IsWithinInclusive(TransitionTime, 0.15f, 0.30f);
}

FVector FUrbanCameraSettings::GetThirdPersonOffset(const EUrbanPerspective Perspective) const
{
    const float ShoulderOffsetY = Perspective == EUrbanPerspective::ThirdPersonLeft
        ? LeftShoulderOffsetY
        : RightShoulderOffsetY;

    return FVector(-ThirdPersonDistance, ShoulderOffsetY, ThirdPersonVerticalOffset);
}
