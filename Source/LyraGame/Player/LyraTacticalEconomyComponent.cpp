// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/LyraTacticalEconomyComponent.h"

ULyraTacticalEconomyComponent::ULyraTacticalEconomyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
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
	return true;
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
