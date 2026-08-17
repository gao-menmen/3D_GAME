#include "Camera/LyraCameraMode_UrbanPerspective.h"

#include "Camera/LyraCameraComponent.h"

#include "Character/UrbanPerspectiveComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCameraMode_UrbanPerspective)

namespace UrbanFirstPersonWeapon
{
    // Actor tag marking a spawned weapon actor that has already been reparented
    // onto the camera. Guarded per actor (not per pawn) so a weapon granted a
    // few frames after possession is still picked up, and a failed early
    // attempt never permanently blocks a later one.
    const FName ComponentTagName(TEXT("Urban.FirstPersonWeapon"));
}

void ULyraCameraMode_UrbanPerspective::EnsureFirstPersonWeapon(AActor* TargetActor)
{
    const APawn* TargetPawn = Cast<APawn>(TargetActor);
    ULyraCameraComponent* CameraComponent = TargetPawn
        ? ULyraCameraComponent::FindCameraComponent(TargetPawn)
        : nullptr;
    USceneComponent* AttachParent = CameraComponent
        ? StaticCast<USceneComponent*>(CameraComponent)
        : TargetActor->GetRootComponent();
    if (!AttachParent)
    {
        return;
    }

    // Reparent every equipped weapon actor (B_Pistol, which carries the Muzzle
    // socket used by the firing GameplayCue) from the hand socket onto the
    // camera, so the muzzle flash and bullet FX spawn at the first-person
    // weapon view instead of at the world body. The camera-relative pose
    // matches the C++ muzzle trace origin (forward 30, right 20, down 6).
    //
    // No static fallback mesh is created: the pistol is always equipped in this
    // game, and a fallback would render as a second, flat-looking gun next to
    // the real one when it won the race against the (slightly delayed)
    // equipment grant.
    if (ULyraEquipmentManagerComponent* EquipManager =
            TargetPawn->FindComponentByClass<ULyraEquipmentManagerComponent>())
    {
        for (ULyraEquipmentInstance* Instance :
             EquipManager->GetEquipmentInstancesOfType(ULyraEquipmentInstance::StaticClass()))
        {
            for (AActor* Spawned : Instance->GetSpawnedActors())
            {
                if (!Spawned)
                {
                    continue;
                }
                if (Spawned->Tags.Contains(UrbanFirstPersonWeapon::ComponentTagName))
                {
                    continue;
                }
                USceneComponent* Root = Spawned->GetRootComponent();
                if (!Root)
                {
                    continue;
                }
                FWeaponAttachmentState& AttachmentState = WeaponAttachmentStates.Add(Spawned);
                AttachmentState.Parent = Root->GetAttachParent();
                AttachmentState.SocketName = Root->GetAttachSocketName();
                AttachmentState.RelativeTransform = Root->GetRelativeTransform();

                Root->SetMobility(EComponentMobility::Movable);
                Root->AttachToComponent(
                    AttachParent,
                    FAttachmentTransformRules::KeepRelativeTransform);
                Root->SetRelativeLocation(FVector(30.0f, 20.0f, -22.0f));
                Root->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
                Spawned->Tags.Add(UrbanFirstPersonWeapon::ComponentTagName);
            }
        }
    }
}

FVector UrbanCameraTransition::ResolveFrameLocation(
    const FVector& TransitionStartLocation,
    const FVector& DesiredLocation,
    const float Alpha,
    TFunctionRef<FVector(const FVector&)> CollisionConstraint)
{
    const FVector InterpolatedLocation = FMath::Lerp(
        TransitionStartLocation,
        DesiredLocation,
        FMath::Clamp(Alpha, 0.0f, 1.0f));
    return CollisionConstraint(InterpolatedLocation);
}

ULyraCameraMode_UrbanPerspective::ULyraCameraMode_UrbanPerspective()
{
    CameraSettings.Sanitize();
    BlendTime = CameraSettings.TransitionTime;
    FieldOfView = CameraSettings.ThirdPersonFieldOfView;
}

bool ULyraCameraMode_UrbanPerspective::IsThirdPerson(const EUrbanPerspective Perspective)
{
    return Perspective == EUrbanPerspective::ThirdPersonRight
        || Perspective == EUrbanPerspective::ThirdPersonLeft;
}

void ULyraCameraMode_UrbanPerspective::BuildThirdPersonView(
    const FUrbanCameraSettings& SafeSettings,
    const EUrbanPerspective Perspective)
{
    const FVector PivotLocation = GetPivotLocation();
    FRotator PivotRotation = GetPivotRotation();
    PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);

    const FVector DesiredLocation = PivotLocation
        + PivotRotation.RotateVector(SafeSettings.GetThirdPersonOffset(Perspective));

    View.Location = ResolveCameraPenetration(
        PivotLocation,
        DesiredLocation,
        SafeSettings.CollisionRadius);
    View.Rotation = PivotRotation;
    View.ControlRotation = PivotRotation;
    View.FieldOfView = SafeSettings.ThirdPersonFieldOfView;
}

void ULyraCameraMode_UrbanPerspective::RestoreWeaponAttachments()
{
    for (auto It = WeaponAttachmentStates.CreateIterator(); It; ++It)
    {
        AActor* WeaponActor = It.Key().Get();
        if (!WeaponActor)
        {
            It.RemoveCurrent();
            continue;
        }

        USceneComponent* Root = WeaponActor->GetRootComponent();
        USceneComponent* OriginalParent = It.Value().Parent.Get();
        if (Root && OriginalParent)
        {
            Root->AttachToComponent(
                OriginalParent,
                FAttachmentTransformRules::KeepRelativeTransform,
                It.Value().SocketName);
            Root->SetRelativeTransform(It.Value().RelativeTransform);
        }
        WeaponActor->Tags.Remove(UrbanFirstPersonWeapon::ComponentTagName);
        It.RemoveCurrent();
    }
}
void ULyraCameraMode_UrbanPerspective::BuildFirstPersonView(
    const FUrbanCameraSettings& SafeSettings)
{
    AActor* TargetActor = GetTargetActor();
    check(TargetActor);

    const APawn* TargetPawn = Cast<APawn>(TargetActor);
    FRotator ViewRotation = GetPivotRotation();
    ViewRotation.Pitch = FMath::ClampAngle(ViewRotation.Pitch, ViewPitchMin, ViewPitchMax);

    View.Location = TargetPawn ? TargetPawn->GetPawnViewLocation() : GetPivotLocation();
    View.Rotation = ViewRotation;
    View.ControlRotation = ViewRotation;
    View.FieldOfView = SafeSettings.FirstPersonFieldOfView;
}

FVector ULyraCameraMode_UrbanPerspective::ResolveCameraPenetration(
    const FVector& PivotLocation,
    const FVector& DesiredLocation,
    const float CollisionRadius) const
{
    UWorld* World = GetWorld();
    AActor* TargetActor = GetTargetActor();
    if (!World || !TargetActor || PivotLocation.Equals(DesiredLocation))
    {
        return DesiredLocation;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(UrbanCameraPenetration), false, TargetActor);
    FHitResult Hit;
    const bool bBlocked = World->SweepSingleByChannel(
        Hit,
        PivotLocation,
        DesiredLocation,
        FQuat::Identity,
        ECC_Camera,
        FCollisionShape::MakeSphere(CollisionRadius),
        QueryParams);

    return bBlocked && Hit.bBlockingHit ? Hit.Location : DesiredLocation;
}

void ULyraCameraMode_UrbanPerspective::ResetTransitionState(
    AActor* TargetActor,
    UUrbanPerspectiveComponent* PerspectiveComponent)
{
    if (PerspectiveComponent)
    {
        PerspectiveComponent->FinishTransition();
    }

    LastTargetActor = TargetActor;
    TransitionElapsed = 0.0f;
    bHasPreviousView = false;
    bTransitionActive = false;
}

void ULyraCameraMode_UrbanPerspective::UpdateView(const float DeltaTime)
{
    AActor* TargetActor = GetTargetActor();
    check(TargetActor);

    UUrbanPerspectiveComponent* PerspectiveComponent =
        TargetActor->FindComponentByClass<UUrbanPerspectiveComponent>();
    const EUrbanPerspective Perspective = PerspectiveComponent
        ? PerspectiveComponent->GetAcceptedPerspective()
        : EUrbanPerspective::FirstPerson;

    if (LastTargetActor.Get() != TargetActor)
    {
        RestoreWeaponAttachments();
        ResetTransitionState(TargetActor, PerspectiveComponent);
    }

    FUrbanCameraSettings SafeSettings = CameraSettings;
    SafeSettings.Sanitize();

    if (IsThirdPerson(Perspective))
    {
        RestoreWeaponAttachments();
        BuildThirdPersonView(SafeSettings, Perspective);
    }
    else
    {
        EnsureFirstPersonWeapon(TargetActor);
        BuildFirstPersonView(SafeSettings);
    }

    const FVector DesiredLocation = View.Location;
    const float DesiredFieldOfView = View.FieldOfView;
    if (!bHasPreviousView)
    {
        LastPerspective = Perspective;
        LastOutputLocation = DesiredLocation;
        LastOutputFieldOfView = DesiredFieldOfView;
        bHasPreviousView = true;
        return;
    }

    if (Perspective != LastPerspective)
    {
        TransitionStartLocation = LastOutputLocation;
        TransitionStartFieldOfView = LastOutputFieldOfView;
        TransitionElapsed = 0.0f;
        bTransitionActive = true;
        LastPerspective = Perspective;

        if (PerspectiveComponent)
        {
            PerspectiveComponent->BeginTransition();
        }
    }

    if (bTransitionActive)
    {
        TransitionElapsed += FMath::Max(DeltaTime, 0.0f);
        const float Alpha = FMath::Clamp(
            TransitionElapsed / SafeSettings.TransitionTime,
            0.0f,
            1.0f);

        const FVector TransitionPivotLocation = GetPivotLocation();
        View.Location = UrbanCameraTransition::ResolveFrameLocation(
            TransitionStartLocation,
            DesiredLocation,
            Alpha,
            [this, TransitionPivotLocation, CollisionRadius = SafeSettings.CollisionRadius](
                const FVector& InterpolatedLocation)
            {
                return ResolveCameraPenetration(
                    TransitionPivotLocation,
                    InterpolatedLocation,
                    CollisionRadius);
            });
        View.FieldOfView = FMath::Lerp(
            TransitionStartFieldOfView,
            DesiredFieldOfView,
            Alpha);

        if (Alpha >= 1.0f)
        {
            bTransitionActive = false;
            if (PerspectiveComponent)
            {
                PerspectiveComponent->FinishTransition();
            }
        }
    }

    LastOutputLocation = View.Location;
    LastOutputFieldOfView = View.FieldOfView;
}
