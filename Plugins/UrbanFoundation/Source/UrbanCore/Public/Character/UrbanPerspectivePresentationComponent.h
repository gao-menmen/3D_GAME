#pragma once

#include "Character/UrbanPerspectiveTypes.h"
#include "Components/PawnComponent.h"
#include "UrbanPerspectivePresentationComponent.generated.h"

class AActor;
class UPrimitiveComponent;
class ULyraPawnComponent_CharacterParts;
class UUrbanPerspectiveComponent;
namespace EEndPlayReason { enum Type : int; }

USTRUCT(BlueprintType)
struct URBANCORE_API FUrbanMuzzleObstructionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Urban|Weapon")
    bool bObstructed = false;

    UPROPERTY(BlueprintReadOnly, Category="Urban|Weapon")
    FVector ImpactPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Urban|Weapon")
    TObjectPtr<AActor> HitActor = nullptr;
};

UCLASS(ClassGroup=(Urban), meta=(BlueprintSpawnableComponent))
class URBANCORE_API UUrbanPerspectivePresentationComponent : public UPawnComponent
{
    GENERATED_BODY()

public:
    UUrbanPerspectivePresentationComponent(
        const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    UFUNCTION(BlueprintPure, Category="Urban|Weapon")
    FUrbanMuzzleObstructionResult TraceMuzzleToAim(
        const FVector& Muzzle,
        const FVector& AimPoint) const;

    UFUNCTION(BlueprintCallable, Category="Urban|Character View")
    void RefreshPresentation();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ResolveComponents();
    void ApplyPerspective(EUrbanPerspective Perspective);
    void ApplyFirstPersonVisibility(UPrimitiveComponent* Component, bool bFirstPerson) const;

    UFUNCTION()
    void HandlePerspectiveChanged(EUrbanPerspective NewPerspective);

    UFUNCTION()
    void HandleCharacterPartsChanged(ULyraPawnComponent_CharacterParts* ChangedComponent);

    UPROPERTY(Transient)
    TObjectPtr<UUrbanPerspectiveComponent> PerspectiveComponent;

    UPROPERTY(Transient)
    TObjectPtr<ULyraPawnComponent_CharacterParts> CharacterPartsComponent;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UPrimitiveComponent>> FirstPersonVisibleBodyComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UPrimitiveComponent>> FirstPersonHiddenBodyComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UPrimitiveComponent>> FirstPersonArmsComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UPrimitiveComponent>> FirstPersonWeaponComponents;

    bool bWarnedMissingFirstPersonArms = false;
};
