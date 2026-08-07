#include "Camera/LyraCameraMode_UrbanADS.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCameraMode_UrbanADS)

ULyraCameraMode_UrbanADS::ULyraCameraMode_UrbanADS()
{
    FieldOfView = ADSFieldOfView;
}

void ULyraCameraMode_UrbanADS::UpdateView(const float DeltaTime)
{
    // Build the standard first-person view, then apply the ADS zoom.
    Super::UpdateView(DeltaTime);
    View.FieldOfView = ADSFieldOfView;
}
