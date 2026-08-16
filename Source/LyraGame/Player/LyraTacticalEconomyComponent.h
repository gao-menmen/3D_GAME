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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Economy")
	int32 GetFunds() const { return Funds; }

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Economy")
	bool CanAfford(int32 Price) const;

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Economy")
	float GetArmor() const { return Armor; }

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Economy")
	bool HasHelmet() const { return bHasHelmet; }

	/** Consumes armor durability after the shared damage model resolves a hit. */
	UFUNCTION(BlueprintCallable, Category="Lyra|Tactical Economy")
	void ApplyArmorDamage(float Damage);

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
	UFUNCTION(Server, Reliable)
	void ServerTryPurchase(int32 Price);

	UFUNCTION(Server, Reliable)
	void ServerTryPurchaseArmor(int32 Price);

	UFUNCTION(Server, Reliable)
	void ServerTryPurchaseHelmet(int32 Price);

	bool ShouldMirrorPurchaseToServer() const;
	bool TryPurchaseArmorInternal(int32 Price);
	bool TryPurchaseHelmetInternal(int32 Price);

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Lyra|Tactical Economy")
	int32 Funds = 4000;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Lyra|Tactical Economy")
	float Armor = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Lyra|Tactical Economy")
	bool bHasHelmet = false;
};
