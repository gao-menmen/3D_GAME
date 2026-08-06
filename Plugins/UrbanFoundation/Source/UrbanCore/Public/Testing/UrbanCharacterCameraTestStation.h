#pragma once

#include "Character/UrbanPerspectiveTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UrbanCharacterCameraTestStation.generated.h"

class APawn;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace EEndPlayReason { enum Type : int; }

UENUM(BlueprintType)
enum class EUrbanCharacterCameraTestStationMode : uint8
{
    AimingRestriction,
    SprintingRestriction,
    TraversalRestriction,
    DownedRestriction,
    ForcedFirstPerson,
    DeathReset,
};

UCLASS(BlueprintType)
class URBANCORE_API AUrbanCharacterCameraTestStation final : public AActor
{
    GENERATED_BODY()

public:
    AUrbanCharacterCameraTestStation(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, Category="Urban|Character Camera Test")
    void SetStationMode(EUrbanCharacterCameraTestStationMode NewMode);

    UFUNCTION(BlueprintCallable, Category="Urban|Character Camera Test")
    void SetTriggerExtent(const FVector& NewExtent);

    UFUNCTION(BlueprintPure, Category="Urban|Character Camera Test")
    EUrbanCharacterCameraTestStationMode GetStationMode() const { return StationMode; }

    UFUNCTION(BlueprintPure, Category="Urban|Character Camera Test")
    FName GetRestrictionTagName() const;

    UFUNCTION(BlueprintCallable, Category="Urban|Character Camera Test")
    bool ApplyToPawn(APawn* Pawn);

    UFUNCTION(BlueprintCallable, Category="Urban|Character Camera Test")
    bool RemoveFromPawn(APawn* Pawn);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Urban|Character Camera Test")
    TObjectPtr<UBoxComponent> Trigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Urban|Character Camera Test")
    TObjectPtr<UStaticMeshComponent> FloorMarker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Urban|Character Camera Test")
    TObjectPtr<UTextRenderComponent> StationLabel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Urban|Character Camera Test")
    FVector TriggerExtent = FVector(180.0, 180.0, 120.0);

    static FName GetRestrictionTagNameForMode(EUrbanCharacterCameraTestStationMode Mode);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleTriggerEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

    void RefreshVisuals();
    static FString GetStationLabel(EUrbanCharacterCameraTestStationMode Mode);
    static FColor GetStationColor(EUrbanCharacterCameraTestStationMode Mode);

    UPROPERTY(EditAnywhere, Category="Urban|Character Camera Test")
    EUrbanCharacterCameraTestStationMode StationMode =
        EUrbanCharacterCameraTestStationMode::AimingRestriction;

    TSet<TWeakObjectPtr<APawn>> ActivePawns;
    TMap<TWeakObjectPtr<APawn>, EUrbanViewPolicy> PreviousPolicies;
};