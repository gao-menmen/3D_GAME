#include "Character/UrbanPerspectiveComponent.h"

#include "Character/UrbanCharacterStateComponent.h"
#include "Character/UrbanViewPolicyComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

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

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ReevaluatePolicy();
    }
}

void UUrbanPerspectiveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ViewPolicyComponent)
    {
        ViewPolicyComponent->OnPolicyChanged.RemoveDynamic(this, &ThisClass::HandlePolicyChanged);
    }

    CharacterStateComponent = nullptr;
    ViewPolicyComponent = nullptr;

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
    EUrbanPerspective Requested = EUrbanPerspective::FirstPerson;
    if (AcceptedPerspective == EUrbanPerspective::FirstPerson
        || AcceptedPerspective == EUrbanPerspective::ForcedFirstPerson)
    {
        Requested = PreferredPerspective == EUrbanPerspective::ThirdPersonLeft
            ? EUrbanPerspective::ThirdPersonLeft
            : EUrbanPerspective::ThirdPersonRight;
    }

    ServerRequestPerspective(Requested);
}

void UUrbanPerspectiveComponent::RequestToggleShoulder()
{
    if (AcceptedPerspective == EUrbanPerspective::ThirdPersonRight)
    {
        ServerRequestPerspective(EUrbanPerspective::ThirdPersonLeft);
    }
    else if (AcceptedPerspective == EUrbanPerspective::ThirdPersonLeft)
    {
        ServerRequestPerspective(EUrbanPerspective::ThirdPersonRight);
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
    ResolveComponents();

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
        SetAcceptedPerspective(Decision.AcceptedPerspective);
        if (Policy == EUrbanViewPolicy::FreeChoice)
        {
            PreferredPerspective = Decision.AcceptedPerspective;
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

    State.bTransitioning = bTransitioning;
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

void UUrbanPerspectiveComponent::OnRep_AcceptedPerspective()
{
    OnPerspectiveChanged.Broadcast(AcceptedPerspective);
}

void UUrbanPerspectiveComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(ThisClass, AcceptedPerspective, COND_OwnerOnly);
}