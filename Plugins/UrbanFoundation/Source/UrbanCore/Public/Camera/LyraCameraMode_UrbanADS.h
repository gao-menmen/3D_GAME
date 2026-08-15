#pragma once

#include "Camera/LyraCameraMode_UrbanPerspective.h"
#include "LyraCameraMode_UrbanADS.generated.h"

/**
 * First-person aim-down-sight camera. Reuses the UrbanPerspective first-person
 * view construction (eye location, collision, pitch clamp) and only zooms the
 * field of view down while aiming.
 */
UCLASS(Blueprintable)
class URBANCORE_API ULyraCameraMode_UrbanADS : public ULyraCameraMode_UrbanPerspective
{
    GENERATED_BODY()

public:
    ULyraCameraMode_UrbanADS();

protected:
    virtual void UpdateView(float DeltaTime) override;

private:
    UPROPERTY(EditDefaultsOnly, Category="Urban|Camera", meta=(ClampMin="30", ClampMax="120"))
    float ADSFieldOfView = 60.0f;
};
