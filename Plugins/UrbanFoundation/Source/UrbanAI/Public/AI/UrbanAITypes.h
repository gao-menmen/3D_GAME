#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UrbanAITypes.generated.h"

UENUM(BlueprintType)
enum class EUrbanEnemyArchetype : uint8
{
    Rifleman,
    Assault,
    Marksman,
    Drone
};

UENUM(BlueprintType)
enum class EUrbanAIBehaviorState : uint8
{
    PatrolOrGuard,
    Investigate,
    Search,
    CoverCombat,
    Flank,
    CallReinforcement
};

USTRUCT(BlueprintType)
struct URBANAI_API FUrbanEnemyArchetypeTuning
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|AI")
    EUrbanEnemyArchetype Archetype = EUrbanEnemyArchetype::Rifleman;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|AI")
    float PreferredEngagementRangeMeters = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|AI")
    float ReactionTimeSeconds = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|AI")
    float AimSpreadDegrees = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|AI")
    float CoverPreference = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|AI")
    float FlankPreference = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|AI")
    bool bCanUseGrenades = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Urban Spear|AI")
    bool bIsAerial = false;

    void Sanitize();
    static FUrbanEnemyArchetypeTuning MakeDefaults(EUrbanEnemyArchetype InArchetype);
};

USTRUCT(BlueprintType)
struct URBANAI_API FUrbanAIDecisionContext
{
    GENERATED_BODY()

    /** True only while perception currently confirms the target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Urban Spear|AI")
    bool bHasConfirmedTarget = false;

    /** A non-expired sight or sound clue exists at a last-known location. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Urban Spear|AI")
    bool bHasRecentStimulus = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Urban Spear|AI")
    bool bReachedLastKnownLocation = false;

    /** Must be supplied by navigation validation; AI may not invent a flank path. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Urban Spear|AI")
    bool bHasValidFlankRoute = false;

    /** Must be supplied by the mission/zone budget; reinforcements are never unlimited. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Urban Spear|AI")
    bool bReinforcementBudgetAvailable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Urban Spear|AI")
    float TimeInStateSeconds = 0.0f;
};

UCLASS()
class URBANAI_API UUrbanAIDecisionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Urban Spear|AI")
    static EUrbanAIBehaviorState ResolveBehaviorState(const FUrbanAIDecisionContext& Context);
};
