#include "Character/UrbanPerspectivePresentationComponent.h"

#include "Character/UrbanPerspectiveComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
#include "Engine/World.h"
#include "GameFramework/PrimitiveComponentUtilities.h"

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
        PerspectiveComponent->OnPerspectiveChanged.AddUniqueDynamic(
            this,
            &ThisClass::HandlePerspectiveChanged);
    }
    if (CharacterPartsComponent)
    {
        CharacterPartsComponent->OnCharacterPartsChanged.AddUniqueDynamic(
            this,
            &ThisClass::HandleCharacterPartsChanged);
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
    if (CharacterPartsComponent)
    {
        CharacterPartsComponent->OnCharacterPartsChanged.RemoveDynamic(
            this,
            &ThisClass::HandleCharacterPartsChanged);
    }

    PerspectiveComponent = nullptr;
    CharacterPartsComponent = nullptr;
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
    if (!CharacterPartsComponent)
    {
        CharacterPartsComponent = Owner->FindComponentByClass<ULyraPawnComponent_CharacterParts>();
    }

    WorldBodyComponents.Reset();
    FirstPersonArmsComponents.Reset();
    FirstPersonWeaponComponents.Reset();

    const auto CollectPrimitiveComponents = [this, Owner](AActor* SourceActor, const bool bTreatUntaggedAsWorldBody)
    {
        if (!SourceActor)
        {
            return;
        }

        TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(SourceActor);
        for (UPrimitiveComponent* Component : PrimitiveComponents)
        {
            if (!Component)
            {
                continue;
            }

            if (bTreatUntaggedAsWorldBody)
            {
                UPrimitiveComponentUtilities::AddVisibilityOwner(Component, Owner);
            }

            if (Component->ComponentHasTag(UrbanPerspectivePresentation::FirstPersonArmsTag))
            {
                FirstPersonArmsComponents.AddUnique(Component);
                continue;
            }
            if (Component->ComponentHasTag(UrbanPerspectivePresentation::FirstPersonWeaponTag))
            {
                FirstPersonWeaponComponents.AddUnique(Component);
                continue;
            }
            if (bTreatUntaggedAsWorldBody
                || Component->ComponentHasTag(UrbanPerspectivePresentation::WorldBodyTag))
            {
                WorldBodyComponents.AddUnique(Component);
            }
        }
    };

    CollectPrimitiveComponents(Owner, false);

    TInlineComponentArray<UChildActorComponent*> ChildActorComponents(Owner);
    for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
    {
        if (ChildActorComponent)
        {
            CollectPrimitiveComponents(ChildActorComponent->GetChildActor(), true);
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

void UUrbanPerspectivePresentationComponent::HandleCharacterPartsChanged(
    ULyraPawnComponent_CharacterParts* ChangedComponent)
{
    if (ChangedComponent == CharacterPartsComponent)
    {
        RefreshPresentation();
    }
}
