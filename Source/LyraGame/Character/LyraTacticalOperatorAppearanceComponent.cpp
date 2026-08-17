// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/LyraTacticalOperatorAppearanceComponent.h"

#include "Components/ChildActorComponent.h"
#include "Components/MeshComponent.h"
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
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
	if (ULyraPawnComponent_CharacterParts* Parts = GetOwner()->FindComponentByClass<ULyraPawnComponent_CharacterParts>())
	{
		Parts->OnCharacterPartsChanged.AddDynamic(this, &ThisClass::OnCharacterPartsChanged);
	}
	ApplyUniformMaterials();
}

void ULyraTacticalOperatorAppearanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ULyraTacticalOperatorAppearanceComponent, AppearanceFlags);
	DOREPLIFETIME(ULyraTacticalOperatorAppearanceComponent, UniformPreset);
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


bool ULyraTacticalOperatorAppearanceComponent::IsValidUniformPreset(const ELyraOperatorUniformPreset Preset)
{
	return Preset == ELyraOperatorUniformPreset::Urban ||
		Preset == ELyraOperatorUniformPreset::Stealth ||
		Preset == ELyraOperatorUniformPreset::Assault;
}

void ULyraTacticalOperatorAppearanceComponent::SetUniformPreset(const ELyraOperatorUniformPreset NewPreset)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidUniformPreset(NewPreset))
	{
		return;
	}
	if (UniformPreset != NewPreset)
	{
		UniformPreset = NewPreset;
		ApplyUniformMaterials();
		GetOwner()->ForceNetUpdate();
	}
}

void ULyraTacticalOperatorAppearanceComponent::OnRep_UniformPreset()
{
	ApplyUniformMaterials();
}

void ULyraTacticalOperatorAppearanceComponent::OnCharacterPartsChanged(ULyraPawnComponent_CharacterParts* ChangedParts)
{
	(void)ChangedParts;
	ApplyUniformMaterials();
}

void ULyraTacticalOperatorAppearanceComponent::ApplyUniformMaterials()
{
	ULyraPawnComponent_CharacterParts* Parts = GetOwner() ? GetOwner()->FindComponentByClass<ULyraPawnComponent_CharacterParts>() : nullptr;
	if (!Parts)
	{
		return;
	}

	FLinearColor CarbonTint(0.018f, 0.027f, 0.032f);
	float EmissiveStrength = 1.35f;
	float MetalBrightness = 0.42f;
	float RubberBrightness = 0.72f;
	float PlasticBrightness = 0.65f;
	if (UniformPreset == ELyraOperatorUniformPreset::Stealth)
	{
		CarbonTint = FLinearColor(0.008f, 0.012f, 0.014f);
		EmissiveStrength = 0.35f;
		MetalBrightness = 0.22f;
		RubberBrightness = 0.42f;
		PlasticBrightness = 0.38f;
	}
	else if (UniformPreset == ELyraOperatorUniformPreset::Assault)
	{
		CarbonTint = FLinearColor(0.035f, 0.045f, 0.050f);
		EmissiveStrength = 2.0f;
		MetalBrightness = 0.52f;
		RubberBrightness = 0.82f;
		PlasticBrightness = 0.78f;
	}

	for (AActor* PartActor : Parts->GetCharacterPartActors())
	{
		if (!PartActor)
		{
			continue;
		}
		TInlineComponentArray<UMeshComponent*> Meshes(PartActor);
		for (UMeshComponent* Mesh : Meshes)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
			{
				if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
				{
					Material->SetVectorParameterValue(TEXT("CarbonfiberTint"), CarbonTint);
					Material->SetScalarParameterValue(TEXT("EmissiveStrength"), EmissiveStrength);
					Material->SetScalarParameterValue(TEXT("MetalBrighness"), MetalBrightness);
					Material->SetScalarParameterValue(TEXT("RubberBrightness"), RubberBrightness);
					Material->SetScalarParameterValue(TEXT("PlasticBrightness"), PlasticBrightness);
				}
			}
		}
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
