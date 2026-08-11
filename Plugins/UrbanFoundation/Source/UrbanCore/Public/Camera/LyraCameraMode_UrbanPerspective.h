#pragma once

#include "Camera/LyraCameraMode.h"
#include "Camera/UrbanCameraSettings.h"
#include "Templates/Function.h"
#include "LyraCameraMode_UrbanPerspective.generated.h"

class UUrbanPerspectiveComponent;

namespace UrbanCameraTransition
{
    URBANCORE_API FVector ResolveFrameLocation(
        const FVector& TransitionStartLocation,
        const FVector& DesiredLocation,
        float Alpha,
        TFunctionRef<FVector(const FVector&)> CollisionConstraint);
}

UCLASS(Blueprintable)
class URBANCORE_API ULyraCameraMode_UrbanPerspective : public ULyraCameraMode
{
    GENERATED_BODY()

public:
    ULyraCameraMode_UrbanPerspective();

    const FUrbanCameraSettings& GetCameraSettings() const { return CameraSettings; }

protected:
    virtual void UpdateView(float DeltaTime) override;

private:
    void EnsureFirstPersonWeapon(AActor* TargetActor);
    static bool IsThirdPerson(EUrbanPerspective Perspective);
    void BuildThirdPersonView(
        const FUrbanCameraSettings& SafeSettings,
        EUrbanPerspective Perspective);
    void BuildFirstPersonView(const FUrbanCameraSettings& SafeSettings);
    FVector ResolveCameraPenetration(
        const FVector& PivotLocation,
        const FVector& DesiredLocation,
        float CollisionRadius) const;
    void ResetTransitionState(AActor* TargetActor, UUrbanPerspectiveComponent* PerspectiveComponent);

    UPROPERTY(EditDefaultsOnly, Category="Urban|Camera")
    FUrbanCameraSettings CameraSettings;

    TWeakObjectPtr<AActor> LastTargetActor;
    FVector TransitionStartLocation = FVector::ZeroVector;
    FVector LastOutputLocation = FVector::ZeroVector;
    float TransitionStartFieldOfView = 90.0f;
    float LastOutputFieldOfView = 90.0f;
    float TransitionElapsed = 0.0f;
    EUrbanPerspective LastPerspective = EUrbanPerspective::FirstPerson;
    bool bHasPreviousView = false;
    bool bTransitionActive = false;
};
