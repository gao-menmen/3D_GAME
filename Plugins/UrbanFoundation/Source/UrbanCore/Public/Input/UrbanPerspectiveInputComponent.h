#pragma once

#include "Components/PawnComponent.h"
#include "UrbanPerspectiveInputComponent.generated.h"

class AController;
class APawn;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UUrbanPerspectiveComponent;
namespace EEndPlayReason { enum Type : int; }

UCLASS(ClassGroup=(Urban), meta=(BlueprintSpawnableComponent))
class URBANCORE_API UUrbanPerspectiveInputComponent : public UPawnComponent
{
    GENERATED_BODY()

public:
    UUrbanPerspectiveInputComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void TryBindInput();
    void RemoveInputBindings();
    void HandleTogglePerspective();
    void HandleToggleShoulder();

    UFUNCTION()
    void HandlePawnRestarted(APawn* Pawn);

    UFUNCTION()
    void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

    UPROPERTY(EditDefaultsOnly, Category="Urban|Input")
    TSoftObjectPtr<UInputAction> TogglePerspectiveAction;

    UPROPERTY(EditDefaultsOnly, Category="Urban|Input")
    TSoftObjectPtr<UInputAction> ToggleShoulderAction;

    UPROPERTY(EditDefaultsOnly, Category="Urban|Input")
    TSoftObjectPtr<UInputMappingContext> PerspectiveMappingContext;

    UPROPERTY(EditDefaultsOnly, Category="Urban|Input")
    int32 MappingPriority = 10;

    UPROPERTY(Transient)
    TObjectPtr<UEnhancedInputComponent> BoundInputComponent;

    UPROPERTY(Transient)
    TObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundInputSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> BoundMappingContext;

    UPROPERTY(Transient)
    TObjectPtr<UUrbanPerspectiveComponent> PerspectiveComponent;

    TArray<uint32> BindingHandles;
};
