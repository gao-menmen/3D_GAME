#pragma once

#include "Character/UrbanPerspectiveTypes.h"
#include "Components/PawnComponent.h"
#include "UrbanPerspectiveComponent.generated.h"

class AController;
class APawn;
class UUrbanCharacterStateComponent;
class UUrbanPerspectivePreferenceComponent;
class UUrbanViewPolicyComponent;
namespace EEndPlayReason { enum Type : int; }

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FUrbanPerspectiveChanged,
    EUrbanPerspective,
    NewPerspective);

UCLASS(ClassGroup=(Urban), meta=(BlueprintSpawnableComponent))
class URBANCORE_API UUrbanPerspectiveComponent : public UPawnComponent
{
    GENERATED_BODY()

public:
    UUrbanPerspectiveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UFUNCTION(BlueprintPure, Category="Urban|Character View")
    EUrbanPerspective GetAcceptedPerspective() const { return AcceptedPerspective; }

    UFUNCTION(BlueprintPure, Category="Urban|Character View")
    EUrbanPerspective GetPreferredPerspective() const { return PreferredPerspective; }

    UFUNCTION(BlueprintPure, Category="Urban|Character View")
    EUrbanPerspective GetPreferredThirdPersonPerspective() const { return PreferredThirdPersonPerspective; }

    UFUNCTION(BlueprintPure, Category="Urban|Character View")
    bool IsTransitioning() const { return bTransitioning; }

    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Urban|Character View")
    void ServerRequestPerspective(EUrbanPerspective Requested);

    UFUNCTION(BlueprintCallable, Category="Urban|Character View")
    void RequestTogglePerspective();

    UFUNCTION(BlueprintCallable, Category="Urban|Character View")
    void RequestToggleShoulder();

    UFUNCTION(BlueprintCallable, Category="Urban|Character View")
    void BeginTransition();

    UFUNCTION(BlueprintCallable, Category="Urban|Character View")
    void FinishTransition();

    UFUNCTION(BlueprintCallable, Category="Urban|Character View")
    void RestoreAfterRespawn();

    UPROPERTY(BlueprintAssignable, Category="Urban|Character View")
    FUrbanPerspectiveChanged OnPerspectiveChanged;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ServerRequestPerspective_Implementation(EUrbanPerspective Requested);

    UFUNCTION(Server, Reliable)
    void ServerTogglePerspective();
    void ServerTogglePerspective_Implementation();

    UFUNCTION(Server, Reliable)
    void ServerToggleShoulder();
    void ServerToggleShoulder_Implementation();

    void ResolveComponents();
    UUrbanPerspectivePreferenceComponent* ResolvePreferenceComponent(bool bCreateOnAuthority);
    void LoadPreference();
    void SavePreference(EUrbanPerspective Perspective);
    void ApplyRequest(EUrbanPerspective Requested);
    void ReevaluatePolicy();
    void SetAcceptedPerspective(EUrbanPerspective NewPerspective);
    void StartServerTransitionLock();
    bool IsServerTransitionLocked() const;
    FUrbanCharacterViewState BuildEvaluationState() const;

    UFUNCTION()
    void HandlePolicyChanged(EUrbanViewPolicy NewPolicy);

    UFUNCTION()
    void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

    UFUNCTION()
    void OnRep_AcceptedPerspective();

    UPROPERTY(ReplicatedUsing=OnRep_AcceptedPerspective)
    EUrbanPerspective AcceptedPerspective = EUrbanPerspective::FirstPerson;

    UPROPERTY(Replicated)
    EUrbanPerspective PreferredPerspective = EUrbanPerspective::FirstPerson;

    UPROPERTY(Replicated)
    EUrbanPerspective PreferredThirdPersonPerspective = EUrbanPerspective::ThirdPersonRight;

    UPROPERTY(EditDefaultsOnly, Category="Urban|Character View", meta=(ClampMin="0.15", ClampMax="0.30"))
    float ServerTransitionDuration = 0.2f;

    UPROPERTY(Transient)
    TObjectPtr<UUrbanCharacterStateComponent> CharacterStateComponent;

    UPROPERTY(Transient)
    TObjectPtr<UUrbanViewPolicyComponent> ViewPolicyComponent;

    UPROPERTY(Transient)
    TObjectPtr<UUrbanPerspectivePreferenceComponent> PreferenceComponent;

    double ServerTransitionLockUntil = 0.0;
    bool bTransitioning = false;
};
