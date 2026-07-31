#pragma once

#include "Character/UrbanPerspectiveTypes.h"
#include "Components/PawnComponent.h"
#include "GameplayTagContainer.h"
#include "UrbanCharacterStateComponent.generated.h"

class ULyraAbilitySystemComponent;

namespace EEndPlayReason { enum Type : int; }

UCLASS(ClassGroup=(Urban), meta=(BlueprintSpawnableComponent))
class URBANCORE_API UUrbanCharacterStateComponent : public UPawnComponent
{
    GENERATED_BODY()

public:
    UUrbanCharacterStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UFUNCTION(BlueprintPure, Category="Urban|Character View")
    FUrbanCharacterViewState GetViewState() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    ULyraAbilitySystemComponent* ResolveAbilitySystem() const;
    FUrbanCharacterViewState BuildViewState(const ULyraAbilitySystemComponent* AbilitySystem) const;
    void RegisterRestrictionDelegates();
    void UnregisterRestrictionDelegates();
    void RefreshViewState();
    void HandleRestrictionTagChanged(const FGameplayTag Tag, int32 NewCount);

    UPROPERTY(Transient)
    TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;

    FUrbanCharacterViewState CachedViewState;
    TMap<FGameplayTag, FDelegateHandle> RestrictionDelegateHandles;
};