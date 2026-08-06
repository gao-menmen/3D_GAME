#include "Character/UrbanPerspectivePreferenceComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UrbanPerspectivePreferenceComponent)

UUrbanPerspectivePreferenceComponent::UUrbanPerspectivePreferenceComponent(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UUrbanPerspectivePreferenceComponent::SaveFreeChoice(const EUrbanPerspective Perspective)
{
    if (Perspective == EUrbanPerspective::FirstPerson
        || Perspective == EUrbanPerspective::ThirdPersonRight
        || Perspective == EUrbanPerspective::ThirdPersonLeft)
    {
        PreferredPerspective = Perspective;
    }

    if (Perspective == EUrbanPerspective::ThirdPersonRight
        || Perspective == EUrbanPerspective::ThirdPersonLeft)
    {
        PreferredThirdPersonPerspective = Perspective;
    }
}
