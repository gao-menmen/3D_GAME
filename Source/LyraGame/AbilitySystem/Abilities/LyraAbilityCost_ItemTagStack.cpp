// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraAbilityCost_ItemTagStack.h"

#include "AbilitySystemComponent.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraGameplayAbility_FromEquipment.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraAbilityCost_ItemTagStack)

UE_DEFINE_GAMEPLAY_TAG(TAG_ABILITY_FAIL_COST, "Ability.ActivateFail.Cost");

ULyraAbilityCost_ItemTagStack::ULyraAbilityCost_ItemTagStack()
{
	Quantity.SetValue(1.0f);
	FailureTag = TAG_ABILITY_FAIL_COST;
}

namespace LyraAbilityCost_ItemTagStack_Impl
{
	// Resolves the item instance this ability's equipment is associated with.
	//
	// The canonical path goes through ULyraGameplayAbility_FromEquipment::
	// GetAssociatedItem(), which reads UGameplayAbility::GetCurrentAbilitySpec().
	// That is an instance-scoped function and asserts when the ability is not
	// yet instantiated - which is exactly the case during the pre-activation
	// cost check, where the engine calls CheckCost on the ability CDO. During
	// that window the spec handle + actor info are the only reliable source of
	// the equipment, so resolve the item from the ASC's spec list instead.
	ULyraInventoryItemInstance* FindAssociatedItem(const ULyraGameplayAbility* Ability,
		const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo)
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			if (const FGameplayAbilitySpec* Spec =
					ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle))
			{
				if (ULyraEquipmentInstance* Equipment =
						Cast<ULyraEquipmentInstance>(Spec->SourceObject.Get()))
				{
					return Cast<ULyraInventoryItemInstance>(Equipment->GetInstigator());
				}
			}
		}

		// Fall back to the canonical path once the ability is instantiated.
		if (const ULyraGameplayAbility_FromEquipment* EquipmentAbility =
				Cast<const ULyraGameplayAbility_FromEquipment>(Ability))
		{
			return EquipmentAbility->GetAssociatedItem();
		}
		return nullptr;
	}
}

bool ULyraAbilityCost_ItemTagStack::CheckCost(const ULyraGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ULyraInventoryItemInstance* ItemInstance =
			LyraAbilityCost_ItemTagStack_Impl::FindAssociatedItem(Ability, Handle, ActorInfo))
	{
		const int32 AbilityLevel = Ability->GetAbilityLevel(Handle, ActorInfo);

		const float NumStacksReal = Quantity.GetValueAtLevel(AbilityLevel);
		const int32 NumStacks = FMath::TruncToInt(NumStacksReal);
		const bool bCanApplyCost = ItemInstance->GetStatTagStackCount(Tag) >= NumStacks;

		// Inform other abilities why this cost cannot be applied
		if (!bCanApplyCost && OptionalRelevantTags && FailureTag.IsValid())
		{
			OptionalRelevantTags->AddTag(FailureTag);				
		}
		return bCanApplyCost;
	}
	return false;
}

void ULyraAbilityCost_ItemTagStack::ApplyCost(const ULyraGameplayAbility* Ability, const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (ActorInfo->IsNetAuthority())
	{
		if (ULyraInventoryItemInstance* ItemInstance =
				LyraAbilityCost_ItemTagStack_Impl::FindAssociatedItem(Ability, Handle, ActorInfo))
		{
			const int32 AbilityLevel = Ability->GetAbilityLevel(Handle, ActorInfo);

			const float NumStacksReal = Quantity.GetValueAtLevel(AbilityLevel);
			const int32 NumStacks = FMath::TruncToInt(NumStacksReal);

			ItemInstance->RemoveStatTagStack(Tag, NumStacks);
		}
	}
}

