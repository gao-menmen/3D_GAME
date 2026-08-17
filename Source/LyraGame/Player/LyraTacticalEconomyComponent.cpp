// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/LyraTacticalEconomyComponent.h"

#include "Net/UnrealNetwork.h"

ULyraTacticalEconomyComponent::ULyraTacticalEconomyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULyraTacticalEconomyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ULyraTacticalEconomyComponent, Funds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ULyraTacticalEconomyComponent, Armor, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ULyraTacticalEconomyComponent, bHasHelmet, COND_OwnerOnly);
}

bool ULyraTacticalEconomyComponent::ShouldMirrorPurchaseToServer() const
{
	return GetOwner() && !GetOwner()->HasAuthority();
}

void ULyraTacticalEconomyComponent::ApplyArmorDamage(const float Damage)
{
	const float OldArmor = Armor;
	Armor = FMath::Max(Armor - FMath::Max(Damage, 0.0f), 0.0f);
	if (!FMath::IsNearlyEqual(OldArmor, Armor))
	{
		BroadcastEquipmentChanged();
	}
}

bool ULyraTacticalEconomyComponent::CanAfford(const int32 Price) const
{
	return Price >= 0 && Funds >= Price;
}

bool ULyraTacticalEconomyComponent::TryPurchase(const int32 Price)
{
	if (!CanAfford(Price))
	{
		return false;
	}
	Funds -= Price;
	if (ShouldMirrorPurchaseToServer())
	{
		ServerTryPurchase(Price);
	}
	return true;
}

bool ULyraTacticalEconomyComponent::TryPurchaseArmorInternal(const int32 Price)
{
	if (Armor >= 100.0f || !CanAfford(Price))
	{
		return false;
	}
	Funds -= Price;
	Armor = 100.0f;
	BroadcastEquipmentChanged();
	return true;
}

bool ULyraTacticalEconomyComponent::TryPurchaseArmor(const int32 Price)
{
	const bool bPurchased = TryPurchaseArmorInternal(Price);
	if (bPurchased && ShouldMirrorPurchaseToServer())
	{
		ServerTryPurchaseArmor(Price);
	}
	return bPurchased;
}

bool ULyraTacticalEconomyComponent::TryPurchaseHelmetInternal(const int32 Price)
{
	if (bHasHelmet || Armor <= 0.0f || !CanAfford(Price))
	{
		return false;
	}
	Funds -= Price;
	bHasHelmet = true;
	BroadcastEquipmentChanged();
	return true;
}

bool ULyraTacticalEconomyComponent::TryPurchaseHelmet(const int32 Price)
{
	const bool bPurchased = TryPurchaseHelmetInternal(Price);
	if (bPurchased && ShouldMirrorPurchaseToServer())
	{
		ServerTryPurchaseHelmet(Price);
	}
	return bPurchased;
}

void ULyraTacticalEconomyComponent::ServerTryPurchase_Implementation(const int32 Price)
{
	TryPurchase(Price);
}

void ULyraTacticalEconomyComponent::ServerTryPurchaseArmor_Implementation(const int32 Price)
{
	TryPurchaseArmorInternal(Price);
}

void ULyraTacticalEconomyComponent::ServerTryPurchaseHelmet_Implementation(const int32 Price)
{
	TryPurchaseHelmetInternal(Price);
}

void ULyraTacticalEconomyComponent::AddKillReward(const int32 Reward)
{
	Funds = ClampFunds(Funds + FMath::Max(Reward, 0));
}

void ULyraTacticalEconomyComponent::AddRoundAward(const bool bWonRound, const int32 ConsecutiveLosses)
{
	Funds = ClampFunds(Funds + ResolveRoundAward(bWonRound, ConsecutiveLosses));
}

int32 ULyraTacticalEconomyComponent::ResolveRoundAward(const bool bWonRound, const int32 ConsecutiveLosses)
{
	if (bWonRound)
	{
		return 3250;
	}
	return 1400 + FMath::Clamp(ConsecutiveLosses, 0, 4) * 500;
}

int32 ULyraTacticalEconomyComponent::ClampFunds(const int32 InFunds)
{
	return FMath::Clamp(InFunds, 0, 16000);
}

void ULyraTacticalEconomyComponent::OnRep_EquipmentState()
{
	BroadcastEquipmentChanged();
}

void ULyraTacticalEconomyComponent::BroadcastEquipmentChanged()
{
	EquipmentChanged.Broadcast();
}
