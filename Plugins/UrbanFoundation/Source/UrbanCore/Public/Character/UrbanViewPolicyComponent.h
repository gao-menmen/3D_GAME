#pragma once

#include "Character/UrbanPerspectiveTypes.h"
#include "Components/ActorComponent.h"
#include "UrbanViewPolicyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FUrbanViewPolicyChanged,
    EUrbanViewPolicy,
    NewPolicy);

UCLASS(ClassGroup=(Urban), meta=(BlueprintSpawnableComponent))
class URBANCORE_API UUrbanViewPolicyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUrbanViewPolicyComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UFUNCTION(BlueprintPure, Category="Urban|Character View")
    EUrbanViewPolicy GetPolicy() const { return Policy; }

    UFUNCTION(BlueprintCallable, Category="Urban|Character View")
    bool SetPolicy(EUrbanViewPolicy NewPolicy);

    UPROPERTY(BlueprintAssignable, Category="Urban|Character View")
    FUrbanViewPolicyChanged OnPolicyChanged;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UFUNCTION()
    void OnRep_Policy();

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Policy, Category="Urban|Character View")
    EUrbanViewPolicy Policy = EUrbanViewPolicy::FreeChoice;
};