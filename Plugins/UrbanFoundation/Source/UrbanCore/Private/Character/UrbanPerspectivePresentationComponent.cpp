#include "Character/UrbanPerspectivePresentationComponent.h"

#include "Character/UrbanPerspectiveComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UrbanPerspectivePresentationComponent)

DEFINE_LOG_CATEGORY_STATIC(LogUrbanPerspectivePresentation, Log, All);

namespace UrbanPerspectivePresentation
{
    const FName WorldBodyTag(TEXT("Urban.WorldBody"));
    const FName FirstPersonArmsTag(TEXT("Urban.FirstPersonArms"));
    const FName FirstPersonWeaponTag(TEXT("Urban.FirstPersonWeapon"));
}

UUrbanPerspectivePresentationComponent::UUrbanPerspectivePresentationComponent(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UUrbanPerspectivePresentationComponent::BeginPlay()
{
    Super::BeginPlay();

    ResolveComponents();
    if (PerspectiveComponent)
    {
        PerspectiveComponent->OnPerspectiveChanged.AddDynamic(
            this,
            &ThisClass::HandlePerspectiveChanged);
    }

    RefreshPresentation();
}

void UUrbanPerspectivePresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PerspectiveComponent)
    {
        PerspectiveComponent->OnPerspectiveChanged.RemoveDynamic(
            this,
            &ThisClass::HandlePerspectiveChanged);
    }

    PerspectiveComponent = nullptr;
    WorldBodyComponents.Reset();
    FirstPersonArmsComponents.Reset();
    FirstPersonWeaponComponents.Reset();

    Super::EndPlay(EndPlayReason);
}

FUrbanMuzzleObstructionResult UUrbanPerspectivePresentationComponent::TraceMuzzleToAim(
    const FVector& Muzzle,
    const FVector& AimPoint) const
{
    FUrbanMuzzleObstructionResult Result;
    const UWorld* World = GetWorld();
    if (!World || Muzzle.Equals(AimPoint))
    {
        return Result;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(UrbanMuzzleObstruction), false);
    if (const AActor* Owner = GetOwner())
    {
        QueryParams.AddIgnoredActor(Owner);
    }

    FHitResult Hit;
    Result.bObstructed = World->LineTraceSingleByChannel(
        Hit,
        Muzzle,
        AimPoint,
        ECC_Visibility,
        QueryParams);
    if (Result.bObstructed)
    {
        Result.ImpactPoint = Hit.ImpactPoint;
        Result.HitActor = Hit.GetActor();
    }

    return Result;
}

void UUrbanPerspectivePresentationComponent::RefreshPresentation()
{
    ResolveComponents();
    const EUrbanPerspective Perspective = PerspectiveComponent
        ? PerspectiveComponent->GetAcceptedPerspective()
        : EUrbanPerspective::FirstPerson;
    ApplyPerspective(Perspective);
}

void UUrbanPerspectivePresentationComponent::ResolveComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    if (!PerspectiveComponent)
    {
        PerspectiveComponent = Owner->FindComponentByClass<UUrbanPerspectiveComponent>();
    }

    WorldBodyComponents.Reset();
    FirstPersonArmsComponents.Reset();
    FirstPersonWeaponComponents.Reset();

    TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Owner);
    for (UPrimitiveComponent* Component : PrimitiveComponents)
    {
        if (!Component)
        {
            continue;
        }

        if (Component->ComponentHasTag(UrbanPerspectivePresentation::WorldBodyTag))
        {
            WorldBodyComponents.Add(Component);
        }
        if (Component->ComponentHasTag(UrbanPerspectivePresentation::FirstPersonArmsTag))
        {
            FirstPersonArmsComponents.Add(Component);
        }
        if (Component->ComponentHasTag(UrbanPerspectivePresentation::FirstPersonWeaponTag))
        {
            FirstPersonWeaponComponents.Add(Component);
        }
    }

    if (FirstPersonArmsComponents.IsEmpty() && !bWarnedMissingFirstPersonArms)
    {
        bWarnedMissingFirstPersonArms = true;
        UE_LOG(
            LogUrbanPerspectivePresentation,
            Warning,
            TEXT("%s has no component tagged Urban.FirstPersonArms; first-person presentation will continue without optional arms."),
            *GetNameSafe(Owner));
    }
}

void UUrbanPerspectivePresentationComponent::ApplyPerspective(const EUrbanPerspective Perspective)
{
    const bool bFirstPerson = Perspective == EUrbanPerspective::FirstPerson
        || Perspective == EUrbanPerspective::ForcedFirstPerson;

    for (UPrimitiveComponent* WorldBody : WorldBodyComponents)
    {
        if (WorldBody)
        {
            WorldBody->SetOwnerNoSee(bFirstPerson);
        }
    }

    for (UPrimitiveComponent* FirstPersonArms : FirstPersonArmsComponents)
    {
        ApplyFirstPersonVisibility(FirstPersonArms, bFirstPerson);
    }
    for (UPrimitiveComponent* FirstPersonWeapon : FirstPersonWeaponComponents)
    {
        ApplyFirstPersonVisibility(FirstPersonWeapon, bFirstPerson);
    }
}

void UUrbanPerspectivePresentationComponent::ApplyFirstPersonVisibility(
    UPrimitiveComponent* Component,
    const bool bFirstPerson) const
{
    if (!Component)
    {
        return;
    }

    Component->SetOnlyOwnerSee(true);
    Component->SetOwnerNoSee(!bFirstPerson);
}

void UUrbanPerspectivePresentationComponent::HandlePerspectiveChanged(
    const EUrbanPerspective NewPerspective)
{
    ApplyPerspective(NewPerspective);
}
