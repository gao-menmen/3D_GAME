// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "LyraTacticalOperatorAppearanceComponent.generated.h"

class UChildActorComponent;

/** Compact public equipment state replicated with the pawn for third-person cosmetics. */
UENUM(BlueprintType)
enum class ELyraTacticalAppearanceFlags : uint8
{
	None = 0,
	Armor = 1 << 0,
	Helmet = 1 << 1,
};
ENUM_CLASS_FLAGS(ELyraTacticalAppearanceFlags);

/**
 * Replicates equipment appearance independently from the owner-only economy data.
 * Optional cosmetic actor classes can be supplied later without changing gameplay code.
 */
UCLASS(ClassGroup=(Lyra), meta=(BlueprintSpawnableComponent))
class LYRAGAME_API ULyraTacticalOperatorAppearanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULyraTacticalOperatorAppearanceComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Appearance")
	bool IsWearingArmor() const { return HasFlag(AppearanceFlags, ELyraTacticalAppearanceFlags::Armor); }

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Appearance")
	bool IsWearingHelmet() const { return HasFlag(AppearanceFlags, ELyraTacticalAppearanceFlags::Helmet); }

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Appearance")
	uint8 GetAppearanceFlags() const { return AppearanceFlags; }

	/** Authority-only bridge from private economy state to public pawn cosmetics. */
	void SetEquipmentState(float Armor, bool bHasHelmet);

	static uint8 MakeAppearanceFlags(float Armor, bool bHasHelmet);

private:
	static bool HasFlag(uint8 Flags, ELyraTacticalAppearanceFlags Flag);

	UFUNCTION()
	void OnRep_AppearanceFlags();

	void RefreshCosmetics();
	void RefreshCosmeticActor(TObjectPtr<UChildActorComponent>& Component, TSubclassOf<AActor> DesiredClass, FName ComponentName, FName SocketName);

	UPROPERTY(ReplicatedUsing=OnRep_AppearanceFlags, VisibleInstanceOnly, Category="Lyra|Tactical Appearance")
	uint8 AppearanceFlags = 0;

	/** Empty by default until licensed production-quality equipment meshes are imported. */
	UPROPERTY(EditDefaultsOnly, Category="Lyra|Tactical Appearance")
	TSubclassOf<AActor> ArmorCosmeticClass;

	UPROPERTY(EditDefaultsOnly, Category="Lyra|Tactical Appearance")
	TSubclassOf<AActor> HelmetCosmeticClass;

	UPROPERTY(EditDefaultsOnly, Category="Lyra|Tactical Appearance")
	FName ArmorSocketName = TEXT("spine_05");

	UPROPERTY(EditDefaultsOnly, Category="Lyra|Tactical Appearance")
	FName HelmetSocketName = TEXT("head");

	UPROPERTY(Transient)
	TObjectPtr<UChildActorComponent> ArmorCosmeticComponent;

	UPROPERTY(Transient)
	TObjectPtr<UChildActorComponent> HelmetCosmeticComponent;
};
