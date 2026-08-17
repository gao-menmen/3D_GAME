// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Character/LyraOperatorCustomizationTypes.h"
#include "LyraTacticalOperatorAppearanceComponent.generated.h"

class UChildActorComponent;
class ULyraPawnComponent_CharacterParts;

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

	UFUNCTION(BlueprintPure, Category="Lyra|Tactical Appearance")
	ELyraOperatorUniformPreset GetUniformPreset() const { return UniformPreset; }

	/** Applies a replicated uniform style without replacing team-identification colors. */
	void SetUniformPreset(ELyraOperatorUniformPreset NewPreset);

	static bool IsValidUniformPreset(ELyraOperatorUniformPreset Preset);

	/** Authority-only bridge from private economy state to public pawn cosmetics. */
	void SetEquipmentState(float Armor, bool bHasHelmet);

	static uint8 MakeAppearanceFlags(float Armor, bool bHasHelmet);

private:
	static bool HasFlag(uint8 Flags, ELyraTacticalAppearanceFlags Flag);

	UFUNCTION()
	void OnRep_AppearanceFlags();

	UFUNCTION()
	void OnRep_UniformPreset();

	UFUNCTION()
	void OnCharacterPartsChanged(ULyraPawnComponent_CharacterParts* ChangedParts);

	void RefreshCosmetics();
	void ApplyUniformMaterials();
	void RefreshCosmeticActor(TObjectPtr<UChildActorComponent>& Component, TSubclassOf<AActor> DesiredClass, FName ComponentName, FName SocketName);

	UPROPERTY(ReplicatedUsing=OnRep_AppearanceFlags, VisibleInstanceOnly, Category="Lyra|Tactical Appearance")
	uint8 AppearanceFlags = 0;

	UPROPERTY(ReplicatedUsing=OnRep_UniformPreset, VisibleInstanceOnly, Category="Lyra|Tactical Appearance")
	ELyraOperatorUniformPreset UniformPreset = ELyraOperatorUniformPreset::Urban;

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
