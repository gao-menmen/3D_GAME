#include "Character/UrbanCharacterStateComponent.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayAbilitySpec.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Urban_Status_Aiming, "Status.Aiming");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Urban_Status_Sprinting, "Status.Sprinting");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Urban_Status_Traversal, "Status.Traversal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Urban_Status_Downed, "Status.Downed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Urban_Status_Death, "Status.Death");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Urban_Ability_ADS, "Ability.Type.Action.ADS");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Urban_Ability_Dash, "Ability.Type.Action.Dash");

namespace
{
    const FGameplayTag RestrictionTags[] = {
        TAG_Urban_Status_Aiming,
        TAG_Urban_Status_Sprinting,
        TAG_Urban_Status_Traversal,
        TAG_Urban_Status_Downed,
        TAG_Urban_Status_Death,
    };

    bool HasActiveAbilityWithTag(
        const ULyraAbilitySystemComponent* AbilitySystem,
        const FGameplayTag AbilityTag)
    {
        if (!AbilitySystem || !AbilityTag.IsValid())
        {
            return false;
        }

        FGameplayTagContainer RequiredTags;
        RequiredTags.AddTag(AbilityTag);

        TArray<FGameplayAbilitySpec*> MatchingSpecs;
        AbilitySystem->GetActivatableGameplayAbilitySpecsByAllMatchingTags(
            RequiredTags,
            MatchingSpecs,
            false);

        for (const FGameplayAbilitySpec* Spec : MatchingSpecs)
        {
            if (Spec && Spec->IsActive())
            {
                return true;
            }
        }

        return false;
    }
}

UUrbanCharacterStateComponent::UUrbanCharacterStateComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UUrbanCharacterStateComponent::BeginPlay()
{
    Super::BeginPlay();

    AbilitySystemComponent = ResolveAbilitySystem();
    RegisterRestrictionDelegates();
    RefreshViewState();
}

void UUrbanCharacterStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterRestrictionDelegates();
    AbilitySystemComponent = nullptr;

    Super::EndPlay(EndPlayReason);
}

FUrbanCharacterViewState UUrbanCharacterStateComponent::GetViewState() const
{
    if (const ULyraAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem())
    {
        return BuildViewState(AbilitySystem);
    }

    return CachedViewState;
}

ULyraAbilitySystemComponent* UUrbanCharacterStateComponent::ResolveAbilitySystem() const
{
    if (AbilitySystemComponent)
    {
        return AbilitySystemComponent;
    }

    return Cast<ULyraAbilitySystemComponent>(
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner(), true));
}

FUrbanCharacterViewState UUrbanCharacterStateComponent::BuildViewState(
    const ULyraAbilitySystemComponent* AbilitySystem) const
{
    FUrbanCharacterViewState Result;
    Result.bTransitioning = CachedViewState.bTransitioning;

    if (AbilitySystem)
    {
        Result.bAiming = AbilitySystem->HasMatchingGameplayTag(TAG_Urban_Status_Aiming)
            || HasActiveAbilityWithTag(AbilitySystem, TAG_Urban_Ability_ADS);
        Result.bSprinting = AbilitySystem->HasMatchingGameplayTag(TAG_Urban_Status_Sprinting)
            || HasActiveAbilityWithTag(AbilitySystem, TAG_Urban_Ability_Dash);
        Result.bTraversal = AbilitySystem->HasMatchingGameplayTag(TAG_Urban_Status_Traversal);
        Result.bDowned = AbilitySystem->HasMatchingGameplayTag(TAG_Urban_Status_Downed);
        Result.bDead = AbilitySystem->HasMatchingGameplayTag(TAG_Urban_Status_Death);
    }

    return Result;
}

void UUrbanCharacterStateComponent::RegisterRestrictionDelegates()
{
    if (!AbilitySystemComponent || !RestrictionDelegateHandles.IsEmpty())
    {
        return;
    }

    for (const FGameplayTag Tag : RestrictionTags)
    {
        FDelegateHandle Handle = AbilitySystemComponent
            ->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange)
            .AddUObject(this, &ThisClass::HandleRestrictionTagChanged);
        RestrictionDelegateHandles.Add(Tag, Handle);
    }
}

void UUrbanCharacterStateComponent::UnregisterRestrictionDelegates()
{
    if (AbilitySystemComponent)
    {
        for (const TPair<FGameplayTag, FDelegateHandle>& Entry : RestrictionDelegateHandles)
        {
            AbilitySystemComponent->UnregisterGameplayTagEvent(
                Entry.Value,
                Entry.Key,
                EGameplayTagEventType::AnyCountChange);
        }
    }

    RestrictionDelegateHandles.Reset();
}

void UUrbanCharacterStateComponent::RefreshViewState()
{
    CachedViewState = BuildViewState(AbilitySystemComponent);
}

void UUrbanCharacterStateComponent::HandleRestrictionTagChanged(const FGameplayTag Tag, int32 NewCount)
{
    (void)Tag;
    (void)NewCount;
    RefreshViewState();
}