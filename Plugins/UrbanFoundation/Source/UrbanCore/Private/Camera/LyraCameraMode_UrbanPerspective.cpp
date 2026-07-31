#include "Camera/LyraCameraMode_UrbanPerspective.h"

#include "Character/UrbanPerspectiveComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCameraMode_UrbanPerspective)

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

        View.Location = FMath::Lerp(TransitionStartLocation, DesiredLocation, Alpha);
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
