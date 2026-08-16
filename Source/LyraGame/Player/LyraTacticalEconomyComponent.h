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

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Economy")
	float GetArmor() const { return Armor; }

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Economy")
	bool HasHelmet() const { return bHasHelmet; }

	/** Deducts a validated non-negative price. Failed purchases never change funds. */
	UFUNCTION(BlueprintCallable, Category="Lyra|Tactical Economy")
	bool TryPurchase(int32 Price);

	/** Buys a full armor vest. Duplicate purchases are rejected without charging. */
	UFUNCTION(BlueprintCallable, Category="Lyra|Tactical Economy")
	bool TryPurchaseArmor(int32 Price = 650);

	/** Adds head protection. It shares the armor durability pool with the vest. */
	UFUNCTION(BlueprintCallable, Category="Lyra|Tactical Economy")
	bool TryPurchaseHelmet(int32 Price = 350);

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

	UPROPERTY(VisibleInstanceOnly, Category="Lyra|Tactical Economy")
	float Armor = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category="Lyra|Tactical Economy")
	bool bHasHelmet = false;
};
