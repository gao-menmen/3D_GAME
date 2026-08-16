// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "LyraTacticalEconomyComponent.generated.h"

/** Tactical-shooter wallet rules used by buy menus and round rewards. */
UCLASS(ClassGroup=(Lyra), meta=(BlueprintSpawnableComponent))
class LYRAGAME_API ULyraTacticalEconomyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULyraTacticalEconomyComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Economy")
	int32 GetFunds() const { return Funds; }

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Economy")
	bool CanAfford(int32 Price) const;

	/** Deducts a validated non-negative price. Failed purchases never change funds. */
	UFUNCTION(BlueprintCallable, Category="Lyra|Tactical Economy")
	bool TryPurchase(int32 Price);

	UFUNCTION(BlueprintCallable, Category="Lyra|Tactical Economy")
	void AddKillReward(int32 Reward = 300);

	UFUNCTION(BlueprintCallable, Category="Lyra|Tactical Economy")
	void AddRoundAward(bool bWonRound, int32 ConsecutiveLosses);

	/** Pure rules are exposed for UI previews and automation tests. */
	static int32 ResolveRoundAward(bool bWonRound, int32 ConsecutiveLosses);
	static int32 ClampFunds(int32 InFunds);

private:
	UPROPERTY(VisibleInstanceOnly, Category="Lyra|Tactical Economy")
	int32 Funds = 4000;
};
