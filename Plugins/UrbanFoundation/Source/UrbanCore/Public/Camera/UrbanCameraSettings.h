#pragma once

#include "Character/UrbanPerspectiveTypes.h"
#include "CoreMinimal.h"
#include "UrbanCameraSettings.generated.h"

USTRUCT(BlueprintType)
struct URBANCORE_API FUrbanCameraSettings
{
    GENERATED_BODY()

    FUrbanCameraSettings();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Camera")
    float FirstPersonFieldOfView;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Camera")
    float ThirdPersonFieldOfView;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Camera")
    float ThirdPersonDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Camera")
    float RightShoulderOffsetY;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Camera")
    float LeftShoulderOffsetY;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Camera")
    float ThirdPersonVerticalOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Camera")
    float CollisionRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Camera")
    float TransitionTime;

    void Sanitize();
    bool IsWithinSafeRanges() const;
    FVector GetThirdPersonOffset(EUrbanPerspective Perspective) const;
};
