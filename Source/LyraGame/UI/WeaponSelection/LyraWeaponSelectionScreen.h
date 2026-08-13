// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"

#include "LyraWeaponSelectionScreen.generated.h"

class UButton;
class ULyraInventoryItemDefinition;
class UTextBlock;
class UVerticalBox;

/**
 * Pre-round weapon selection screen shown when a player first becomes
 * gameplay-ready. Built entirely from C++ (no blueprint graph needed): a
 * title plus three buttons, one per ShooterCore weapon. Clicking a button
 * adds that weapon's InventoryItemDefinition to the player's inventory,
 * slots it into the quick bar (next free slot) and activates it, leaving the
 * pistol in slot 0 so it can always be switched back to.
 *
 * Shown via CreateWidget + AddToViewport (plain UMG, no CommonUI layer stack)
 * and the owning player controller switches to UI-only input while it is up.
 * A 15s timer falls back to the pistol so the player can never be stuck.
 */
UCLASS(Blueprintable)
class ULyraWeaponSelectionScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	ULyraWeaponSelectionScreen(const FObjectInitializer& ObjectInitializer);

	//~UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~End of UUserWidget interface

protected:
	UFUNCTION()
	void OnPistolClicked();

	UFUNCTION()
	void OnRifleClicked();

	UFUNCTION()
	void OnShotgunClicked();

	UFUNCTION()
	void OnSelectionTimeout();

	void SelectWeapon(TSoftClassPtr<ULyraInventoryItemDefinition> ItemDefClass);
	void BuildMenu();
	UButton* MakeButton(const FText& Label, const FText& Description);
	void RestoreGameInput();

	UPROPERTY(EditDefaultsOnly, Category = "Lyra|WeaponSelection")
	TSoftClassPtr<ULyraInventoryItemDefinition> PistolItemDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Lyra|WeaponSelection")
	TSoftClassPtr<ULyraInventoryItemDefinition> RifleItemDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Lyra|WeaponSelection")
	TSoftClassPtr<ULyraInventoryItemDefinition> ShotgunItemDefinition;

private:
	FTimerHandle SelectionTimerHandle;
	bool bSelectionMade = false;
};
