#include "Camera/LyraCameraMode_UrbanPerspective.h"

#include "Camera/LyraCameraComponent.h"
#include "Character/UrbanPerspectiveComponent.h"
#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCameraMode_UrbanPerspective)

namespace UrbanFirstPersonWeapon
{
    const FName ComponentTagName(TEXT("Urban.FirstPersonWeapon"));
    const FName ComponentName(TEXT("UrbanFPWeapon"));
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

void ULyraCameraMode_UrbanPerspective::EnsureFirstPersonWeapon(AActor* TargetActor)
{
    if (!TargetActor || TargetActor->Tags.Contains(UrbanFirstPersonWeapon::ComponentTagName))
    {
        return;
    }

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

    // Reparent the equipped weapon actor (B_Pistol, which carries the Muzzle
    // socket used by the firing GameplayCue) from the hand socket to the
    // camera, so the muzzle flash and bullet FX spawn at the first-person
    // weapon view instead of at the world body. The camera-relative pose
    // matches the C++ muzzle trace origin (forward 30, right 20, down 6).
    bool bReparentedWeapon = false;
    if (ULyraEquipmentManagerComponent* EquipManager =
            TargetPawn->FindComponentByClass<ULyraEquipmentManagerComponent>())
    {
        ULyraEquipmentInstance* Instance = EquipManager->GetFirstInstanceOfType(
            ULyraEquipmentInstance::StaticClass());
        if (Instance)
        {
            for (AActor* Spawned : Instance->GetSpawnedActors())
            {
                if (!Spawned)
                {
                    continue;
                }
                USceneComponent* Root = Spawned->GetRootComponent();
                if (!Root)
                {
                    continue;
                }
                Root->SetMobility(EComponentMobility::Movable);
                Root->AttachToComponent(
                    AttachParent,
                    FAttachmentTransformRules::KeepRelativeTransform);
                Root->SetRelativeLocation(FVector(30.0f, 20.0f, -6.0f));
                Root->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
                bReparentedWeapon = true;
            }
        }
    }

    if (!bReparentedWeapon)
    {
        // Fallback: no equipped weapon actor found, draw a static pistol so the
        // player still has a visible first-person weapon reference.
        UStaticMeshComponent* WeaponMesh = NewObject<UStaticMeshComponent>(
            TargetActor,
            UrbanFirstPersonWeapon::ComponentName,
            RF_Transient);
        if (WeaponMesh)
        {
            static UStaticMesh* PistolMesh = LoadObject<UStaticMesh>(
                nullptr,
                TEXT("/Game/Weapons/Pistol/Mesh/SM_Pistol.SM_Pistol"));
            if (PistolMesh)
            {
                WeaponMesh->SetStaticMesh(PistolMesh);
            }
            WeaponMesh->SetupAttachment(AttachParent);
            WeaponMesh->SetRelativeLocation(FVector(30.0f, 20.0f, -6.0f));
            WeaponMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
            WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            WeaponMesh->SetCastShadow(false);
            WeaponMesh->ComponentTags.Add(UrbanFirstPersonWeapon::ComponentTagName);
            WeaponMesh->RegisterComponent();
        }
    }

    TargetActor->Tags.Add(UrbanFirstPersonWeapon::ComponentTagName);
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

    EnsureFirstPersonWeapon(TargetActor);

    UUrbanPerspectiveComponent* PerspectiveComponent =
        TargetActor->FindComponentByClass<UUrbanPerspectiveComponent>();
    const EUrbanPerspective Perspective = PerspectiveComponent
        ? PerspectiveComponent->GetAcceptedPerspective()
        : EUrbanPerspective::FirstPerson;

    if (LastTargetActor.Get() != TargetActor)
    {
        ResetTransitionState(TargetActor, PerspectiveComponent);
    }

    FUrbanCameraSettings SafeSettings = CameraSettings;
    SafeSettings.Sanitize();

    if (IsThirdPerson(Perspective))
    {
        BuildThirdPersonView(SafeSettings, Perspective);
    }
    else
    {
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
