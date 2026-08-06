#pragma once

#include "CoreMinimal.h"
#include "UrbanPerspectiveTypes.generated.h"

UENUM(BlueprintType)
enum class EUrbanPerspective : uint8
{
    FirstPerson,
    ThirdPersonRight,
    ThirdPersonLeft,
    ForcedFirstPerson,
};

UENUM(BlueprintType)
enum class EUrbanViewPolicy : uint8
{
    FreeChoice,
    FirstPersonOnly,
    ThirdPersonOnly,
    Disabled,
};

UENUM(BlueprintType)
enum class EUrbanPerspectiveBlockReason : uint8
{
    None,
    Aiming,
    Sprinting,
    Traversal,
    Downed,
    Dead,
    Transitioning,
    Policy,
    Uninitialized,
    InvalidRequest,
};

USTRUCT(BlueprintType)
struct URBANCORE_API FUrbanCharacterViewState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bInitialized = true;

    UPROPERTY(BlueprintReadOnly)
    bool bAiming = false;

    UPROPERTY(BlueprintReadOnly)
    bool bSprinting = false;

    UPROPERTY(BlueprintReadOnly)
    bool bTraversal = false;

    UPROPERTY(BlueprintReadOnly)
    bool bDowned = false;

    UPROPERTY(BlueprintReadOnly)
    bool bDead = false;

    UPROPERTY(BlueprintReadOnly)
    bool bTransitioning = false;
};

USTRUCT(BlueprintType)
struct URBANCORE_API FUrbanPerspectiveDecision
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bAccepted = false;

    UPROPERTY(BlueprintReadOnly)
    EUrbanPerspective AcceptedPerspective = EUrbanPerspective::FirstPerson;

    UPROPERTY(BlueprintReadOnly)
    EUrbanPerspectiveBlockReason Reason = EUrbanPerspectiveBlockReason::None;
};

class URBANCORE_API FUrbanPerspectiveRules
{
public:
    static FUrbanPerspectiveDecision Evaluate(
        EUrbanPerspective Current,
        EUrbanPerspective Requested,
        EUrbanViewPolicy Policy,
        const FUrbanCharacterViewState& State);
};
