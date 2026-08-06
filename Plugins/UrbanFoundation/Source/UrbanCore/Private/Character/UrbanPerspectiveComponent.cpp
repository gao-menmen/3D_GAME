#include "Character/UrbanPerspectiveComponent.h"

#include "Character/UrbanCharacterStateComponent.h"
#include "Character/UrbanPerspectivePreferenceComponent.h"
#include "Character/UrbanViewPolicyComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UrbanPerspectiveComponent)

namespace
{
    const FName UrbanPerspectivePreferenceComponentName(TEXT("UrbanPerspectivePreference"));
}

UUrbanPerspectiveComponent::UUrbanPerspectiveComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UUrbanPerspectiveComponent::BeginPlay()
{
    Super::BeginPlay();

    ResolveComponents();
    if (ViewPolicyComponent)
    {
        ViewPolicyComponent->OnPolicyChanged.AddDynamic(this, &ThisClass::HandlePolicyChanged);
    }

    if (APawn* Pawn = GetPawn<APawn>())
    {
        Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &ThisClass::HandleControllerChanged);
    }

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        RestoreAfterRespawn();
    }
}

void UUrbanPerspectiveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ViewPolicyComponent)
    {
        ViewPolicyComponent->OnPolicyChanged.RemoveDynamic(this, &ThisClass::HandlePolicyChanged);
    }

    if (APawn* Pawn = GetPawn<APawn>())
    {
        Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleControllerChanged);
    }

    CharacterStateComponent = nullptr;
    ViewPolicyComponent = nullptr;
    PreferenceComponent = nullptr;

    Super::EndPlay(EndPlayReason);
}

void UUrbanPerspectiveComponent::ServerRequestPerspective_Implementation(
    const EUrbanPerspective Requested)
{
    ResolveComponents();
    ApplyRequest(Requested);
}

void UUrbanPerspectiveComponent::RequestTogglePerspective()
{
    ServerTogglePerspective();
}

void UUrbanPerspectiveComponent::RequestToggleShoulder()
{
    ServerToggleShoulder();
}

void UUrbanPerspectiveComponent::ServerTogglePerspective_Implementation()
{
    ResolveComponents();

    const EUrbanPerspective Requested = AcceptedPerspective == EUrbanPerspective::FirstPerson
        || AcceptedPerspective == EUrbanPerspective::ForcedFirstPerson
        ? PreferredThirdPersonPerspective
        : EUrbanPerspective::FirstPerson;
    ApplyRequest(Requested);
}

void UUrbanPerspectiveComponent::ServerToggleShoulder_Implementation()
{
    ResolveComponents();

    if (AcceptedPerspective == EUrbanPerspective::ThirdPersonRight)
    {
        ApplyRequest(EUrbanPerspective::ThirdPersonLeft);
    }
    else if (AcceptedPerspective == EUrbanPerspective::ThirdPersonLeft)
    {
        ApplyRequest(EUrbanPerspective::ThirdPersonRight);
    }
}

void UUrbanPerspectiveComponent::BeginTransition()
{
    bTransitioning = true;
}

void UUrbanPerspectiveComponent::FinishTransition()
{
    bTransitioning = false;
}

void UUrbanPerspectiveComponent::RestoreAfterRespawn()
{
    bTransitioning = false;
    ServerTransitionLockUntil = 0.0;
    ResolveComponents();
    LoadPreference();

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ReevaluatePolicy();
    }
}

void UUrbanPerspectiveComponent::ResolveComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    if (!CharacterStateComponent)
    {
        CharacterStateComponent = Owner->FindComponentByClass<UUrbanCharacterStateComponent>();
    }
    if (!ViewPolicyComponent)
    {
        ViewPolicyComponent = Owner->FindComponentByClass<UUrbanViewPolicyComponent>();
    }
    if (!PreferenceComponent)
    {
        PreferenceComponent = ResolvePreferenceComponent(true);
    }
}

UUrbanPerspectivePreferenceComponent* UUrbanPerspectiveComponent::ResolvePreferenceComponent(
    const bool bCreateOnAuthority)
{
    APawn* Pawn = GetPawn<APawn>();
    AController* Controller = Pawn ? Pawn->GetController() : nullptr;
    if (!Controller)
    {
        return nullptr;
    }

    if (UUrbanPerspectivePreferenceComponent* Existing =
        Controller->FindComponentByClass<UUrbanPerspectivePreferenceComponent>())
    {
        return Existing;
    }

    if (!bCreateOnAuthority || !Controller->HasAuthority())
    {
        return nullptr;
    }

    UUrbanPerspectivePreferenceComponent* Created =
        NewObject<UUrbanPerspectivePreferenceComponent>(
            Controller,
            UUrbanPerspectivePreferenceComponent::StaticClass(),
            UrbanPerspectivePreferenceComponentName);
    Controller->AddInstanceComponent(Created);
    Created->RegisterComponent();
    return Created;
}

void UUrbanPerspectiveComponent::LoadPreference()
{
    PreferenceComponent = ResolvePreferenceComponent(true);
    if (PreferenceComponent)
    {
        PreferredPerspective = PreferenceComponent->GetPreferredPerspective();
        PreferredThirdPersonPerspective =
            PreferenceComponent->GetPreferredThirdPersonPerspective();
    }
}

void UUrbanPerspectiveComponent::SavePreference(const EUrbanPerspective Perspective)
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

    PreferenceComponent = ResolvePreferenceComponent(true);
    if (PreferenceComponent)
    {
        PreferenceComponent->SaveFreeChoice(Perspective);
        LoadPreference();
    }
}

void UUrbanPerspectiveComponent::ApplyRequest(const EUrbanPerspective Requested)
{
    const EUrbanViewPolicy Policy = ViewPolicyComponent
        ? ViewPolicyComponent->GetPolicy()
        : EUrbanViewPolicy::FreeChoice;
    const FUrbanPerspectiveDecision Decision = FUrbanPerspectiveRules::Evaluate(
        AcceptedPerspective,
        Requested,
        Policy,
        BuildEvaluationState());

    if (Decision.bAccepted)
    {
        const bool bPerspectiveChanged = AcceptedPerspective != Decision.AcceptedPerspective;
        SetAcceptedPerspective(Decision.AcceptedPerspective);
        if (Policy == EUrbanViewPolicy::FreeChoice)
        {
            SavePreference(Decision.AcceptedPerspective);
        }
        if (bPerspectiveChanged)
        {
            StartServerTransitionLock();
        }
    }
    else if (Decision.Reason == EUrbanPerspectiveBlockReason::Policy)
    {
        SetAcceptedPerspective(Decision.AcceptedPerspective);
    }
}

void UUrbanPerspectiveComponent::ReevaluatePolicy()
{
    bTransitioning = false;
    ServerTransitionLockUntil = 0.0;

    const EUrbanViewPolicy Policy = ViewPolicyComponent
        ? ViewPolicyComponent->GetPolicy()
        : EUrbanViewPolicy::FreeChoice;
    const FUrbanPerspectiveDecision Decision = FUrbanPerspectiveRules::Evaluate(
        AcceptedPerspective,
        PreferredPerspective,
        Policy,
        BuildEvaluationState());

    if (Decision.bAccepted || Decision.Reason == EUrbanPerspectiveBlockReason::Policy)
    {
        SetAcceptedPerspective(Decision.AcceptedPerspective);
    }
}

void UUrbanPerspectiveComponent::SetAcceptedPerspective(const EUrbanPerspective NewPerspective)
{
    if (AcceptedPerspective != NewPerspective)
    {
        AcceptedPerspective = NewPerspective;
        OnPerspectiveChanged.Broadcast(AcceptedPerspective);
    }
}

void UUrbanPerspectiveComponent::StartServerTransitionLock()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (const UWorld* World = GetWorld())
        {
            ServerTransitionLockUntil = World->GetTimeSeconds() + ServerTransitionDuration;
        }
    }
}

bool UUrbanPerspectiveComponent::IsServerTransitionLocked() const
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    return World && World->GetTimeSeconds() < ServerTransitionLockUntil;
}

FUrbanCharacterViewState UUrbanPerspectiveComponent::BuildEvaluationState() const
{
    FUrbanCharacterViewState State;
    if (CharacterStateComponent)
    {
        State = CharacterStateComponent->GetViewState();
    }
    else
    {
        State.bInitialized = false;
    }
    State.bTransitioning = State.bTransitioning || bTransitioning || IsServerTransitionLocked();
    return State;
}

void UUrbanPerspectiveComponent::HandlePolicyChanged(const EUrbanViewPolicy NewPolicy)
{
    (void)NewPolicy;
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ReevaluatePolicy();
    }
}

void UUrbanPerspectiveComponent::HandleControllerChanged(
    APawn* Pawn,
    AController* OldController,
    AController* NewController)
{
    (void)OldController;
    if (Pawn == GetOwner() && NewController && GetOwner()->HasAuthority())
    {
        PreferenceComponent = nullptr;
        RestoreAfterRespawn();
    }
}

void UUrbanPerspectiveComponent::OnRep_AcceptedPerspective()
{
    OnPerspectiveChanged.Broadcast(AcceptedPerspective);
}

void UUrbanPerspectiveComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, AcceptedPerspective);
    DOREPLIFETIME_CONDITION(ThisClass, PreferredPerspective, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ThisClass, PreferredThirdPersonPerspective, COND_OwnerOnly);
}
