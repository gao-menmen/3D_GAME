// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/LyraTacticalOperatorAppearanceComponent.h"

#include "Components/ChildActorComponent.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTacticalOperatorAppearanceComponent)

ULyraTacticalOperatorAppearanceComponent::ULyraTacticalOperatorAppearanceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULyraTacticalOperatorAppearanceComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshCosmetics();
}

void ULyraTacticalOperatorAppearanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ULyraTacticalOperatorAppearanceComponent, AppearanceFlags);
}

uint8 ULyraTacticalOperatorAppearanceComponent::MakeAppearanceFlags(const float Armor, const bool bHasHelmet)
{
	uint8 Result = 0;
	if (Armor > 0.0f)
	{
		Result |= static_cast<uint8>(ELyraTacticalAppearanceFlags::Armor);
	}
	if (bHasHelmet)
	{
		Result |= static_cast<uint8>(ELyraTacticalAppearanceFlags::Helmet);
	}
	return Result;
}

bool ULyraTacticalOperatorAppearanceComponent::HasFlag(const uint8 Flags, const ELyraTacticalAppearanceFlags Flag)
{
	return (Flags & static_cast<uint8>(Flag)) != 0;
}

void ULyraTacticalOperatorAppearanceComponent::SetEquipmentState(const float Armor, const bool bHasHelmet)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const uint8 NewFlags = MakeAppearanceFlags(Armor, bHasHelmet);
	if (AppearanceFlags != NewFlags)
	{
		AppearanceFlags = NewFlags;
		RefreshCosmetics();
		GetOwner()->ForceNetUpdate();
	}
}

void ULyraTacticalOperatorAppearanceComponent::OnRep_AppearanceFlags()
{
	RefreshCosmetics();
}

void ULyraTacticalOperatorAppearanceComponent::RefreshCosmetics()
{
	RefreshCosmeticActor(ArmorCosmeticComponent, IsWearingArmor() ? ArmorCosmeticClass : nullptr, TEXT("TacticalArmorCosmetic"), ArmorSocketName);
	RefreshCosmeticActor(HelmetCosmeticComponent, IsWearingHelmet() ? HelmetCosmeticClass : nullptr, TEXT("TacticalHelmetCosmetic"), HelmetSocketName);
}

void ULyraTacticalOperatorAppearanceComponent::RefreshCosmeticActor(TObjectPtr<UChildActorComponent>& Component, TSubclassOf<AActor> DesiredClass, const FName ComponentName, const FName SocketName)
{
	if (!DesiredClass)
	{
		if (Component)
		{
			Component->DestroyComponent();
			Component = nullptr;
		}
		return;
	}

	if (!Component)
	{
		AActor* Owner = GetOwner();
		ACharacter* Character = Cast<ACharacter>(Owner);
		if (!Owner || !Character || !Character->GetMesh())
		{
			return;
		}

		Component = NewObject<UChildActorComponent>(Owner, ComponentName);
		Component->SetupAttachment(Character->GetMesh(), SocketName);
		Component->RegisterComponent();
	}

	if (Component->GetChildActorClass() != DesiredClass)
	{
		Component->SetChildActorClass(DesiredClass);
	}
}
