#include "Combat/UrbanAmmoComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UUrbanAmmoComponent::UUrbanAmmoComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UUrbanAmmoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UUrbanAmmoComponent, AmmoState);
}

bool UUrbanAmmoComponent::ConfigureAuthoritative(const FUrbanWeaponTuning& WeaponTuning)
{
    if (!HasServerAuthority())
    {
        return false;
    }

    FUrbanWeaponTuning SafeTuning = WeaponTuning;
    SafeTuning.Sanitize();
    AmmoState.Initialize(SafeTuning.MagazineCapacity, SafeTuning.MagazineCapacity, SafeTuning.StartingReserveAmmo);
    BroadcastAmmoStateChanged();
    return true;
}

bool UUrbanAmmoComponent::TryConsumeRoundAuthoritative()
{
    if (!HasServerAuthority() || !AmmoState.TryConsumeRound())
    {
        return false;
    }

    BroadcastAmmoStateChanged();
    return true;
}

bool UUrbanAmmoComponent::BeginReloadAuthoritative()
{
    if (!HasServerAuthority() || !AmmoState.BeginReload())
    {
        return false;
    }

    BroadcastAmmoStateChanged();
    return true;
}

int32 UUrbanAmmoComponent::CompleteReloadAuthoritative()
{
    if (!HasServerAuthority())
    {
        return 0;
    }

    const int32 TransferredRounds = AmmoState.CompleteReload();
    if (TransferredRounds > 0)
    {
        BroadcastAmmoStateChanged();
    }
    return TransferredRounds;
}

bool UUrbanAmmoComponent::CancelReloadAuthoritative()
{
    if (!HasServerAuthority() || !AmmoState.bReloading)
    {
        return false;
    }

    AmmoState.CancelReload();
    BroadcastAmmoStateChanged();
    return true;
}

void UUrbanAmmoComponent::OnRep_AmmoState()
{
    AmmoState.Sanitize();
    BroadcastAmmoStateChanged();
}

bool UUrbanAmmoComponent::HasServerAuthority() const
{
    const AActor* OwnerActor = GetOwner();
    return OwnerActor != nullptr && OwnerActor->HasAuthority();
}

void UUrbanAmmoComponent::BroadcastAmmoStateChanged()
{
    OnAmmoStateChanged.Broadcast();
}
