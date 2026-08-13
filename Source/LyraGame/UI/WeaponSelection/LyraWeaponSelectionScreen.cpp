// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/WeaponSelection/LyraWeaponSelectionScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Equipment/LyraQuickBarComponent.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "LyraLogChannels.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeaponSelectionScreen)

namespace UrbanWeaponSelection
{
	const FText TitleText = NSLOCTEXT("UrbanWeaponSelection", "Title", "选择武器");
	const FText SubtitleText = NSLOCTEXT("UrbanWeaponSelection", "Subtitle", "挑选你的初始武器，开始战斗");
	const FText PistolText = NSLOCTEXT("UrbanWeaponSelection", "Pistol", "手枪");
	const FText RifleText = NSLOCTEXT("UrbanWeaponSelection", "Rifle", "步枪");
	const FText ShotgunText = NSLOCTEXT("UrbanWeaponSelection", "Shotgun", "霰弹枪");
	const FText PistolDesc = NSLOCTEXT("UrbanWeaponSelection", "PistolDesc", "均衡 · 中近距离");
	const FText RifleDesc = NSLOCTEXT("UrbanWeaponSelection", "RifleDesc", "连射 · 中远距离");
	const FText ShotgunDesc = NSLOCTEXT("UrbanWeaponSelection", "ShotgunDesc", "爆发 · 近距离");
	const TCHAR* PistolPath = TEXT("/ShooterCore/Weapons/Pistol/ID_Pistol.ID_Pistol_C");
	const TCHAR* RiflePath = TEXT("/ShooterCore/Weapons/Rifle/ID_Rifle.ID_Rifle_C");
	const TCHAR* ShotgunPath = TEXT("/ShooterCore/Weapons/Shotgun/ID_Shotgun.ID_Shotgun_C");

	// A solid-color brush that needs no texture asset. DrawAs::Box renders a
	// flat rectangle using only TintColor, so nothing has to be loaded at
	// runtime (the stock WhiteTexture asset is not cooked into packaged builds
	// and previously produced a "未找到Object" warning plus an invisible
	// backdrop).
	FSlateBrush MakeSolidBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Color);
		Brush.ImageSize = FVector2D(8.0f, 8.0f);
		return Brush;
	}
}

ULyraWeaponSelectionScreen::ULyraWeaponSelectionScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Focusable so SetKeyboardFocus works and the 1/2/3 hotkeys reach
	// NativeOnKeyDown while the widget is up.
	SetIsFocusable(true);

	PistolItemDefinition = TSoftClassPtr<ULyraInventoryItemDefinition>(FSoftObjectPath(UrbanWeaponSelection::PistolPath));
	RifleItemDefinition = TSoftClassPtr<ULyraInventoryItemDefinition>(FSoftObjectPath(UrbanWeaponSelection::RiflePath));
	ShotgunItemDefinition = TSoftClassPtr<ULyraInventoryItemDefinition>(FSoftObjectPath(UrbanWeaponSelection::ShotgunPath));
}

void ULyraWeaponSelectionScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Build the widget tree here, before the widget's Slate tree is created
	// (the first TakeWidget). Modifying the tree later - e.g. in
	// NativeConstruct, which runs after the Slate tree already exists - would
	// not rebuild the visible UI.
	BuildMenu();
}

void ULyraWeaponSelectionScreen::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: NativeConstruct."));

	// Grab keyboard focus so the 1/2/3 hotkeys below are reachable even if
	// mouse clicking is unavailable for any reason.
	SetKeyboardFocus();

	// Safety net: if the player never picks a weapon (or the UI cannot be
	// seen for any reason), fall back to the pistol and release input so the
	// player is never stuck unable to move.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SelectionTimerHandle,
			this,
			&ThisClass::OnSelectionTimeout,
			15.0f,
			false);
	}
}

FReply ULyraWeaponSelectionScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::One || Key == EKeys::NumPadOne)
	{
		UE_LOG(LogLyra, Log, TEXT("WeaponSelection: hotkey 1 (pistol)."));
		OnPistolClicked();
		return FReply::Handled();
	}
	if (Key == EKeys::Two || Key == EKeys::NumPadTwo)
	{
		UE_LOG(LogLyra, Log, TEXT("WeaponSelection: hotkey 2 (rifle)."));
		OnRifleClicked();
		return FReply::Handled();
	}
	if (Key == EKeys::Three || Key == EKeys::NumPadThree)
	{
		UE_LOG(LogLyra, Log, TEXT("WeaponSelection: hotkey 3 (shotgun)."));
		OnShotgunClicked();
		return FReply::Handled();
	}
	if (Key == EKeys::Enter)
	{
		// Enter picks the first (pistol) option by default.
		UE_LOG(LogLyra, Log, TEXT("WeaponSelection: hotkey Enter (pistol)."));
		OnPistolClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void ULyraWeaponSelectionScreen::BuildMenu()
{
	if (!WidgetTree)
	{
		UE_LOG(LogLyra, Error, TEXT("WeaponSelection: WidgetTree is null, cannot build menu."));
		return;
	}

	// Full-screen root canvas.
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	if (!Canvas)
	{
		return;
	}

	// Semi-transparent dark backdrop covering the whole screen so the menu is
	// unmistakable and nothing behind it steals focus.
	UImage* Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Backdrop"));
	if (Backdrop)
	{
		Backdrop->SetBrush(UrbanWeaponSelection::MakeSolidBrush(FLinearColor(0.015f, 0.015f, 0.04f, 0.82f)));
		if (UCanvasPanelSlot* BackSlot = Canvas->AddChildToCanvas(Backdrop))
		{
			BackSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BackSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	// Centered column with the title and the three weapon buttons.
	UVerticalBox* MenuBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	if (!MenuBox)
	{
		WidgetTree->RootWidget = Canvas;
		return;
	}
	if (UCanvasPanelSlot* MenuBoxSlot = Canvas->AddChildToCanvas(MenuBox))
	{
		MenuBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		MenuBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MenuBoxSlot->SetAutoSize(true);
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	if (Title)
	{
		Title->SetText(UrbanWeaponSelection::TitleText);
		Title->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 48, TEXT("Bold")));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		Title->SetShadowOffset(FVector2D(2.0f, 2.0f));
		if (UVerticalBoxSlot* TitleBoxSlot = MenuBox->AddChildToVerticalBox(Title))
		{
			TitleBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
	}

	UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Subtitle"));
	if (Subtitle)
	{
		Subtitle->SetText(UrbanWeaponSelection::SubtitleText);
		Subtitle->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 20));
		Subtitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.78f, 0.85f)));
		Subtitle->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* SubSlot = MenuBox->AddChildToVerticalBox(Subtitle))
		{
			SubSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 36.0f));
		}
	}

	UButton* PistolButton = MakeButton(UrbanWeaponSelection::PistolText, UrbanWeaponSelection::PistolDesc);
	UButton* RifleButton = MakeButton(UrbanWeaponSelection::RifleText, UrbanWeaponSelection::RifleDesc);
	UButton* ShotgunButton = MakeButton(UrbanWeaponSelection::ShotgunText, UrbanWeaponSelection::ShotgunDesc);
	if (PistolButton)
	{
		PistolButton->OnClicked.AddDynamic(this, &ThisClass::OnPistolClicked);
		if (UVerticalBoxSlot* BtnSlot = MenuBox->AddChildToVerticalBox(PistolButton))
		{
			BtnSlot->SetPadding(FMargin(0.0f, 12.0f));
		}
	}
	if (RifleButton)
	{
		RifleButton->OnClicked.AddDynamic(this, &ThisClass::OnRifleClicked);
		if (UVerticalBoxSlot* BtnSlot = MenuBox->AddChildToVerticalBox(RifleButton))
		{
			BtnSlot->SetPadding(FMargin(0.0f, 12.0f));
		}
	}
	if (ShotgunButton)
	{
		ShotgunButton->OnClicked.AddDynamic(this, &ThisClass::OnShotgunClicked);
		if (UVerticalBoxSlot* BtnSlot = MenuBox->AddChildToVerticalBox(ShotgunButton))
		{
			BtnSlot->SetPadding(FMargin(0.0f, 12.0f));
		}
	}

	// Hotkey hint so the player knows keyboard input works even if the mouse
	// cannot click the buttons for any reason.
	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
	if (Hint)
	{
		Hint->SetText(FText::FromString(TEXT("按 1/2/3 选择，或点击按钮")));
		Hint->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 20));
		Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.75f)));
		Hint->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* HintSlot = MenuBox->AddChildToVerticalBox(Hint))
		{
			HintSlot->SetPadding(FMargin(0.0f, 30.0f, 0.0f, 0.0f));
		}
	}

	WidgetTree->RootWidget = Canvas;
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: menu built (title + 3 buttons + hint)."));
}

UButton* ULyraWeaponSelectionScreen::MakeButton(const FText& Label, const FText& Description)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("WeaponButton"));
	if (!Button)
	{
		return nullptr;
	}

	// Explicit look so the buttons are clearly visible and clickable; width
	// comes from the label text and the button padding.
	Button->SetBackgroundColor(FLinearColor(0.10f, 0.12f, 0.20f, 0.95f));
	Button->SetColorAndOpacity(FLinearColor::White);

	// Two-line content: the weapon name (large) and a one-line trait summary
	// (small, dimmer) stacked vertically.
	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WeaponButtonContent"));

	UTextBlock* LabelBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WeaponButtonLabel"));
	if (LabelBlock)
	{
		LabelBlock->SetText(Label);
		LabelBlock->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 30));
		LabelBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		LabelBlock->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* LabelSlot = ContentBox->AddChildToVerticalBox(LabelBlock))
		{
			LabelSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
			LabelSlot->SetPadding(FMargin(20.0f, 12.0f, 20.0f, 2.0f));
		}
	}

	UTextBlock* DescBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WeaponButtonDesc"));
	if (DescBlock)
	{
		DescBlock->SetText(Description);
		DescBlock->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 16));
		DescBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.68f, 0.78f)));
		DescBlock->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* DescSlot = ContentBox->AddChildToVerticalBox(DescBlock))
		{
			DescSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
			DescSlot->SetPadding(FMargin(20.0f, 0.0f, 20.0f, 12.0f));
		}
	}

	Button->SetContent(ContentBox);

	return Button;
}

void ULyraWeaponSelectionScreen::OnPistolClicked()
{
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: pistol clicked."));
	SelectWeapon(PistolItemDefinition);
}

void ULyraWeaponSelectionScreen::OnRifleClicked()
{
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: rifle clicked."));
	SelectWeapon(RifleItemDefinition);
}

void ULyraWeaponSelectionScreen::OnShotgunClicked()
{
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: shotgun clicked."));
	SelectWeapon(ShotgunItemDefinition);
}

void ULyraWeaponSelectionScreen::OnSelectionTimeout()
{
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: 15s timeout, falling back to pistol."));
	SelectWeapon(PistolItemDefinition);
}

void ULyraWeaponSelectionScreen::SelectWeapon(TSoftClassPtr<ULyraInventoryItemDefinition> ItemDefClass)
{
	if (bSelectionMade)
	{
		return;
	}
	if (ItemDefClass.IsNull())
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: ItemDefClass is null."));
		RestoreGameInput();
		return;
	}
	UClass* LoadedClass = ItemDefClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: failed to load item definition %s."), *ItemDefClass.ToString());
		RestoreGameInput();
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: no owning player controller."));
		RestoreGameInput();
		return;
	}
	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: no pawn on controller."));
		RestoreGameInput();
		return;
	}
	// The inventory manager is a plain actor component; in this game it lives
	// on the player controller next to the quick bar, so search the controller
	// first and fall back to the pawn.
	ULyraInventoryManagerComponent* Inventory = PC->FindComponentByClass<ULyraInventoryManagerComponent>();
	if (!Inventory)
	{
		Inventory = Pawn->FindComponentByClass<ULyraInventoryManagerComponent>();
	}
	ULyraQuickBarComponent* QuickBar = PC->FindComponentByClass<ULyraQuickBarComponent>();
	if (!Inventory || !QuickBar)
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: missing InventoryManager (%d) or QuickBar (%d) component."),
			Inventory != nullptr, QuickBar != nullptr);
		RestoreGameInput();
		return;
	}

	bSelectionMade = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	ULyraInventoryItemInstance* NewItem = Inventory->AddItemDefinition(LoadedClass, 1);
	if (!NewItem)
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: AddItemDefinition failed for %s."), *LoadedClass->GetName());
		RestoreGameInput();
		return;
	}

	// Slot the chosen weapon into the next free quick bar slot and activate
	// it. The pistol stays in slot 0, so the player can switch back with the
	// number keys or the mouse wheel.
	const int32 SlotIndex = QuickBar->GetNextFreeItemSlot();
	QuickBar->AddItemToSlot(SlotIndex, NewItem);
	QuickBar->SetActiveSlotIndex(SlotIndex);

	UE_LOG(LogLyra, Log, TEXT("Weapon selection: equipped %s in quick bar slot %d"),
		*LoadedClass->GetName(), SlotIndex);

	RestoreGameInput();
}

void ULyraWeaponSelectionScreen::RestoreGameInput()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}
