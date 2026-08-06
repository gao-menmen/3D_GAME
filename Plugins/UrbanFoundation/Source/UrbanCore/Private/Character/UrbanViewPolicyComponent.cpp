#include "Character/UrbanViewPolicyComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UUrbanViewPolicyComponent::UUrbanViewPolicyComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

bool UUrbanViewPolicyComponent::SetPolicy(const EUrbanViewPolicy NewPolicy)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return false;
    }

    if (Policy != NewPolicy)
    {
        Policy = NewPolicy;
        OnPolicyChanged.Broadcast(Policy);
    }

    return true;
}

void UUrbanViewPolicyComponent::OnRep_Policy()
{
    OnPolicyChanged.Broadcast(Policy);
}

void UUrbanViewPolicyComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ThisClass, Policy);
}