#pragma once

#include "Character/UrbanPerspectiveTypes.h"
#include "Components/ControllerComponent.h"
#include "UrbanPerspectivePreferenceComponent.generated.h"

UCLASS(ClassGroup=(Urban))
class URBANCORE_API UUrbanPerspectivePreferenceComponent : public UControllerComponent
{
    GENERATED_BODY()

public:
    UUrbanPerspectivePreferenceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    EUrbanPerspective GetPreferredPerspective() const { return PreferredPerspective; }
    EUrbanPerspective GetPreferredThirdPersonPerspective() const { return PreferredThirdPersonPerspective; }

    void SaveFreeChoice(EUrbanPerspective Perspective);

private:
    UPROPERTY(Transient)
    EUrbanPerspective PreferredPerspective = EUrbanPerspective::FirstPerson;

    UPROPERTY(Transient)
    EUrbanPerspective PreferredThirdPersonPerspective = EUrbanPerspective::ThirdPersonRight;
};
