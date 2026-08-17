// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/WeaponSelection/LyraWeaponSelectionScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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
#include "Player/LyraTacticalEconomyComponent.h"
#include "Player/LyraPlayerController.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeaponSelectionScreen)

namespace UrbanWeaponSelection
{
	const FText TitleText = NSLOCTEXT("UrbanWeaponSelection", "Title", "TACTICAL BUY MENU");
	const FText SubtitleText = NSLOCTEXT("UrbanWeaponSelection", "Subtitle", "Choose a loadout before deployment");
	const FText PistolText = NSLOCTEXT("UrbanWeaponSelection", "Pistol", "[1] SIDEARM  |  $0");
	const FText RifleText = NSLOCTEXT("UrbanWeaponSelection", "Rifle", "[2] RIFLE  |  $2700");
	const FText ShotgunText = NSLOCTEXT("UrbanWeaponSelection", "Shotgun", "[3] SHOTGUN  |  $1800");
	const FText ArmorText = NSLOCTEXT("UrbanWeaponSelection", "Armor", "[4] BODY ARMOR  |  $650");
	const FText HelmetText = NSLOCTEXT("UrbanWeaponSelection", "Helmet", "[5] HELMET  |  $350");
	const FText MannyText = NSLOCTEXT("UrbanWeaponSelection", "Manny", "[6] BODY: MANNY");
	const FText QuinnText = NSLOCTEXT("UrbanWeaponSelection", "Quinn", "[7] BODY: QUINN");
	const FText UrbanText = NSLOCTEXT("UrbanWeaponSelection", "Urban", "[8] UNIFORM: URBAN");
	const FText StealthText = NSLOCTEXT("UrbanWeaponSelection", "Stealth", "[9] UNIFORM: STEALTH");
	const FText AssaultText = NSLOCTEXT("UrbanWeaponSelection", "Assault", "[0] UNIFORM: ASSAULT");
	const FText PistolDesc = NSLOCTEXT("UrbanWeaponSelection", "PistolDesc", "Free fallback / balanced at close range");
	const FText RifleDesc = NSLOCTEXT("UrbanWeaponSelection", "RifleDesc", "Automatic / reliable at medium range");
	const FText ShotgunDesc = NSLOCTEXT("UrbanWeaponSelection", "ShotgunDesc", "High impact / close range");
	const FText ArmorDesc = NSLOCTEXT("UrbanWeaponSelection", "ArmorDesc", "100 durability / reduces torso damage");
	const FText HelmetDesc = NSLOCTEXT("UrbanWeaponSelection", "HelmetDesc", "Requires armor / reduces headshot damage");
	const FText MannyDesc = NSLOCTEXT("UrbanWeaponSelection", "MannyDesc", "Masculine Lyra operator body");
	const FText QuinnDesc = NSLOCTEXT("UrbanWeaponSelection", "QuinnDesc", "Feminine Lyra operator body");
	const FText UrbanDesc = NSLOCTEXT("UrbanWeaponSelection", "UrbanDesc", "Balanced city tactical finish");
	const FText StealthDesc = NSLOCTEXT("UrbanWeaponSelection", "StealthDesc", "Low-visibility matte finish");
	const FText AssaultDesc = NSLOCTEXT("UrbanWeaponSelection", "AssaultDesc", "High-contrast combat finish");
	const TCHAR* PistolPath = TEXT("/ShooterCore/Weapons/Pistol/ID_Pistol.ID_Pistol_C");
	const TCHAR* RiflePath = TEXT("/ShooterCore/Weapons/Rifle/ID_Rifle.ID_Rifle_C");
	const TCHAR* ShotgunPath = TEXT("/ShooterCore/Weapons/Shotgun/ID_Shotgun.ID_Shotgun_C");

	// A solid-color brush that needs no texture asset. DrawAs::Box renders a
	// flat rectangle using only TintColor, so nothing has to be loaded at
	// runtime (the stock WhiteTexture asset is not cooked into packaged builds
	// and previously produced a missing-object warning plus an invisible
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
	// Focusable so SetKeyboardFocus works and the 1-5 hotkeys reach
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

	// Grab keyboard focus so the 1-5 hotkeys below are reachable even if
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

int32 ULyraWeaponSelectionScreen::ResolveSelectionIndex(const FKey& Key)
{
	if (Key == EKeys::One || Key == EKeys::NumPadOne || Key == EKeys::Enter)
	{
		return 0;
	}
	if (Key == EKeys::Two || Key == EKeys::NumPadTwo)
	{
		return 1;
	}
	if (Key == EKeys::Three || Key == EKeys::NumPadThree)
	{
		return 2;
	}
	if (Key == EKeys::Four || Key == EKeys::NumPadFour)
	{
		return 3;
	}
	if (Key == EKeys::Five || Key == EKeys::NumPadFive)
	{
		return 4;
	}
	if (Key == EKeys::Six || Key == EKeys::NumPadSix) return 5;
	if (Key == EKeys::Seven || Key == EKeys::NumPadSeven) return 6;
	if (Key == EKeys::Eight || Key == EKeys::NumPadEight) return 7;
	if (Key == EKeys::Nine || Key == EKeys::NumPadNine) return 8;
	if (Key == EKeys::Zero || Key == EKeys::NumPadZero) return 9;
	return INDEX_NONE;
}

FName ULyraWeaponSelectionScreen::GetSelectionShownTag()
{
	return FName(TEXT("Urban.WeaponSelectionShown"));
}

int32 ULyraWeaponSelectionScreen::ResolveWeaponPrice(const int32 SelectionIndex)
{
	switch (SelectionIndex)
	{
	case 0: return 0;
	case 1: return 2700;
	case 2: return 1800;
	case 3: return 650;
	case 4: return 350;
	default: return INDEX_NONE;
	}
}

FReply ULyraWeaponSelectionScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	switch (ResolveSelectionIndex(InKeyEvent.GetKey()))
	{
	case 0:
		OnPistolClicked();
		return FReply::Handled();
	case 1:
		OnRifleClicked();
		return FReply::Handled();
	case 2:
		OnShotgunClicked();
		return FReply::Handled();
	case 3:
		OnArmorClicked();
		return FReply::Handled();
	case 4:
		OnHelmetClicked();
		return FReply::Handled();
	case 5: OnMannyClicked(); return FReply::Handled();
	case 6: OnQuinnClicked(); return FReply::Handled();
	case 7: OnUrbanUniformClicked(); return FReply::Handled();
	case 8: OnStealthUniformClicked(); return FReply::Handled();
	case 9: OnAssaultUniformClicked(); return FReply::Handled();
	default:
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
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
	UButton* ArmorButton = MakeButton(UrbanWeaponSelection::ArmorText, UrbanWeaponSelection::ArmorDesc);
	UButton* HelmetButton = MakeButton(UrbanWeaponSelection::HelmetText, UrbanWeaponSelection::HelmetDesc);
	UButton* MannyButton = MakeButton(UrbanWeaponSelection::MannyText, UrbanWeaponSelection::MannyDesc);
	UButton* QuinnButton = MakeButton(UrbanWeaponSelection::QuinnText, UrbanWeaponSelection::QuinnDesc);
	UButton* UrbanButton = MakeButton(UrbanWeaponSelection::UrbanText, UrbanWeaponSelection::UrbanDesc);
	UButton* StealthButton = MakeButton(UrbanWeaponSelection::StealthText, UrbanWeaponSelection::StealthDesc);
	UButton* AssaultButton = MakeButton(UrbanWeaponSelection::AssaultText, UrbanWeaponSelection::AssaultDesc);
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
	if (ArmorButton)
	{
		ArmorButton->OnClicked.AddDynamic(this, &ThisClass::OnArmorClicked);
		if (UVerticalBoxSlot* BtnSlot = MenuBox->AddChildToVerticalBox(ArmorButton))
		{
			BtnSlot->SetPadding(FMargin(0.0f, 8.0f));
		}
	}
	if (HelmetButton)
	{
		HelmetButton->OnClicked.AddDynamic(this, &ThisClass::OnHelmetClicked);
		if (UVerticalBoxSlot* BtnSlot = MenuBox->AddChildToVerticalBox(HelmetButton))
		{
			BtnSlot->SetPadding(FMargin(0.0f, 8.0f));
		}
	}
	UHorizontalBox* BodyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BodyCustomizationRow"));
	UHorizontalBox* UniformRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("UniformCustomizationRow"));
	auto AddCustomizationButton = [](UHorizontalBox* Row, UButton* Button)
	{
		if (Row && Button)
		{
			if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Button))
			{
				Slot->SetPadding(FMargin(5.0f));
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
	};
	if (MannyButton) MannyButton->OnClicked.AddDynamic(this, &ThisClass::OnMannyClicked);
	if (QuinnButton) QuinnButton->OnClicked.AddDynamic(this, &ThisClass::OnQuinnClicked);
	if (UrbanButton) UrbanButton->OnClicked.AddDynamic(this, &ThisClass::OnUrbanUniformClicked);
	if (StealthButton) StealthButton->OnClicked.AddDynamic(this, &ThisClass::OnStealthUniformClicked);
	if (AssaultButton) AssaultButton->OnClicked.AddDynamic(this, &ThisClass::OnAssaultUniformClicked);
	AddCustomizationButton(BodyRow, MannyButton);
	AddCustomizationButton(BodyRow, QuinnButton);
	AddCustomizationButton(UniformRow, UrbanButton);
	AddCustomizationButton(UniformRow, StealthButton);
	AddCustomizationButton(UniformRow, AssaultButton);
	if (UVerticalBoxSlot* RowSlot = MenuBox->AddChildToVerticalBox(BodyRow)) RowSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	if (UVerticalBoxSlot* RowSlot = MenuBox->AddChildToVerticalBox(UniformRow)) RowSlot->SetPadding(FMargin(0.0f, 2.0f));

	// Hotkey hint so the player knows keyboard input works even if the mouse
	// cannot click the buttons for any reason.
	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Hint"));
	if (Hint)
	{
		Hint->SetText(FText::FromString(TEXT("1-5 BUY  |  6-7 BODY  |  8-0 UNIFORM  |  customization persists after respawn")));
		Hint->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 20));
		Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.75f)));
		Hint->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* HintSlot = MenuBox->AddChildToVerticalBox(Hint))
		{
			HintSlot->SetPadding(FMargin(0.0f, 30.0f, 0.0f, 0.0f));
		}
	}

	WidgetTree->RootWidget = Canvas;
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: menu built (weapons, equipment, and operator customization)."));
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
	SelectWeapon(PistolItemDefinition, ResolveWeaponPrice(0));
}

void ULyraWeaponSelectionScreen::OnRifleClicked()
{
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: rifle clicked."));
	SelectWeapon(RifleItemDefinition, ResolveWeaponPrice(1));
}

void ULyraWeaponSelectionScreen::OnShotgunClicked()
{
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: shotgun clicked."));
	SelectWeapon(ShotgunItemDefinition, ResolveWeaponPrice(2));
}

void ULyraWeaponSelectionScreen::OnArmorClicked()
{
	PurchaseArmor(false);
}

void ULyraWeaponSelectionScreen::OnHelmetClicked()
{
	PurchaseArmor(true);
}

ULyraTacticalEconomyComponent* ULyraWeaponSelectionScreen::FindOrAddEconomyComponent() const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return nullptr;
	}

	return PC->FindComponentByClass<ULyraTacticalEconomyComponent>();
}

void ULyraWeaponSelectionScreen::PurchaseArmor(const bool bHelmet)
{
	ULyraTacticalEconomyComponent* Economy = FindOrAddEconomyComponent();
	const int32 Price = ResolveWeaponPrice(bHelmet ? 4 : 3);
	const bool bPurchased = Economy && (bHelmet
		? Economy->TryPurchaseHelmet(Price)
		: Economy->TryPurchaseArmor(Price));

	UE_LOG(LogLyra, Log,
		TEXT("WeaponSelection: %s purchase %s; armor=%.0f helmet=%d funds=%d"),
		bHelmet ? TEXT("helmet") : TEXT("armor"),
		bPurchased ? TEXT("succeeded") : TEXT("denied"),
		Economy ? Economy->GetArmor() : 0.0f,
		Economy ? Economy->HasHelmet() : false,
		Economy ? Economy->GetFunds() : 0);

	// Equipment purchases are additive. Keep the menu focused so the player
	// can still buy the companion protection item and choose a weapon.
	SetKeyboardFocus();
}


void ULyraWeaponSelectionScreen::OnMannyClicked()
{
	if (ALyraPlayerController* PC = Cast<ALyraPlayerController>(GetOwningPlayer())) PC->RequestOperatorBody(ELyraOperatorBodyType::Manny);
	SetKeyboardFocus();
}

void ULyraWeaponSelectionScreen::OnQuinnClicked()
{
	if (ALyraPlayerController* PC = Cast<ALyraPlayerController>(GetOwningPlayer())) PC->RequestOperatorBody(ELyraOperatorBodyType::Quinn);
	SetKeyboardFocus();
}

void ULyraWeaponSelectionScreen::OnUrbanUniformClicked()
{
	if (ALyraPlayerController* PC = Cast<ALyraPlayerController>(GetOwningPlayer())) PC->RequestOperatorUniform(ELyraOperatorUniformPreset::Urban);
	SetKeyboardFocus();
}

void ULyraWeaponSelectionScreen::OnStealthUniformClicked()
{
	if (ALyraPlayerController* PC = Cast<ALyraPlayerController>(GetOwningPlayer())) PC->RequestOperatorUniform(ELyraOperatorUniformPreset::Stealth);
	SetKeyboardFocus();
}

void ULyraWeaponSelectionScreen::OnAssaultUniformClicked()
{
	if (ALyraPlayerController* PC = Cast<ALyraPlayerController>(GetOwningPlayer())) PC->RequestOperatorUniform(ELyraOperatorUniformPreset::Assault);
	SetKeyboardFocus();
}
void ULyraWeaponSelectionScreen::OnSelectionTimeout()
{
	UE_LOG(LogLyra, Log, TEXT("WeaponSelection: 15s timeout, falling back to pistol."));
	SelectWeapon(PistolItemDefinition, ResolveWeaponPrice(0));
}

void ULyraWeaponSelectionScreen::SelectWeapon(TSoftClassPtr<ULyraInventoryItemDefinition> ItemDefClass, const int32 Price)
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
	ULyraTacticalEconomyComponent* Economy = FindOrAddEconomyComponent();
	if (!Economy || !Economy->TryPurchase(Price))
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: purchase denied, price=%d funds=%d."),
			Price, Economy ? Economy->GetFunds() : 0);
		return; // Keep the buy menu open so the player can choose an affordable item.
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: no pawn on controller."));
		Economy->AddKillReward(Price);
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
		Economy->AddKillReward(Price);
		RestoreGameInput();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	ULyraInventoryItemInstance* NewItem = Inventory->AddItemDefinition(LoadedClass, 1);
	if (!NewItem)
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: AddItemDefinition failed for %s."), *LoadedClass->GetName());
		Economy->AddKillReward(Price);
		RestoreGameInput();
		return;
	}

	// Slot the chosen weapon into the next free quick bar slot and activate
	// it. The pistol stays in slot 0, so the player can switch back with the
	// number keys or the mouse wheel.
	const int32 SlotIndex = QuickBar->GetNextFreeItemSlot();
	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogLyra, Warning, TEXT("WeaponSelection: quick bar is full; cannot equip %s."), *LoadedClass->GetName());
		Inventory->RemoveItemInstance(NewItem);
		Economy->AddKillReward(Price);
		RestoreGameInput();
		return;
	}

	QuickBar->AddItemToSlot(SlotIndex, NewItem);
	QuickBar->SetActiveSlotIndex(SlotIndex);

	bSelectionMade = true;
	UE_LOG(LogLyra, Log, TEXT("Weapon selection: equipped %s in slot %d for $%d; funds=$%d"),
		*LoadedClass->GetName(), SlotIndex, Price, Economy->GetFunds());

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
