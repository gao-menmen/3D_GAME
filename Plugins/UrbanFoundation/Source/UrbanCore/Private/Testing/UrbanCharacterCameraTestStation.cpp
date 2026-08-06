#include "Testing/UrbanCharacterCameraTestStation.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"
#include "Character/UrbanViewPolicyComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

AUrbanCharacterCameraTestStation::AUrbanCharacterCameraTestStation(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
    SetRootComponent(Trigger);
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionObjectType(ECC_WorldDynamic);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->SetGenerateOverlapEvents(true);
    Trigger->SetCanEverAffectNavigation(false);

    FloorMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMarker"));
    FloorMarker->SetupAttachment(Trigger);
    FloorMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FloorMarker->SetGenerateOverlapEvents(false);
    FloorMarker->SetCanEverAffectNavigation(false);
    FloorMarker->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        FloorMarker->SetStaticMesh(CubeMesh.Object);
    }

    StationLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StationLabel"));
    StationLabel->SetupAttachment(Trigger);
    StationLabel->SetHorizontalAlignment(EHTA_Center);
    StationLabel->SetWorldSize(48.0f);
    StationLabel->SetCastShadow(false);
    StationLabel->SetCanEverAffectNavigation(false);

    RefreshVisuals();
}

void AUrbanCharacterCameraTestStation::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshVisuals();
}

void AUrbanCharacterCameraTestStation::BeginPlay()
{
    Super::BeginPlay();

    Trigger->OnComponentBeginOverlap.AddUniqueDynamic(
        this,
        &ThisClass::HandleTriggerBeginOverlap);
    Trigger->OnComponentEndOverlap.AddUniqueDynamic(
        this,
        &ThisClass::HandleTriggerEndOverlap);
}

void AUrbanCharacterCameraTestStation::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    const TArray<TWeakObjectPtr<APawn>> Pawns = ActivePawns.Array();
    for (const TWeakObjectPtr<APawn>& Pawn : Pawns)
    {
        if (Pawn.IsValid())
        {
            RemoveFromPawn(Pawn.Get());
        }
    }

    ActivePawns.Reset();
    PreviousPolicies.Reset();
    Super::EndPlay(EndPlayReason);
}

void AUrbanCharacterCameraTestStation::SetStationMode(
    const EUrbanCharacterCameraTestStationMode NewMode)
{
    if (StationMode != NewMode)
    {
        StationMode = NewMode;
        RefreshVisuals();
    }
}

void AUrbanCharacterCameraTestStation::SetTriggerExtent(const FVector& NewExtent)
{
    TriggerExtent = NewExtent;
    RefreshVisuals();
}

FName AUrbanCharacterCameraTestStation::GetRestrictionTagName() const
{
    return GetRestrictionTagNameForMode(StationMode);
}

FName AUrbanCharacterCameraTestStation::GetRestrictionTagNameForMode(
    const EUrbanCharacterCameraTestStationMode Mode)
{
    switch (Mode)
    {
    case EUrbanCharacterCameraTestStationMode::AimingRestriction:
        return TEXT("Status.Aiming");
    case EUrbanCharacterCameraTestStationMode::SprintingRestriction:
        return TEXT("Status.Sprinting");
    case EUrbanCharacterCameraTestStationMode::TraversalRestriction:
        return TEXT("Status.Traversal");
    case EUrbanCharacterCameraTestStationMode::DownedRestriction:
        return TEXT("Status.Downed");
    default:
        return NAME_None;
    }
}

bool AUrbanCharacterCameraTestStation::ApplyToPawn(APawn* Pawn)
{
    if (!IsValid(Pawn) || !HasAuthority())
    {
        return false;
    }

    if (StationMode == EUrbanCharacterCameraTestStationMode::ForcedFirstPerson)
    {
        UUrbanViewPolicyComponent* Policy =
            Pawn->FindComponentByClass<UUrbanViewPolicyComponent>();
        if (!Policy)
        {
            return false;
        }

        const TWeakObjectPtr<APawn> PawnKey(Pawn);
        PreviousPolicies.FindOrAdd(PawnKey, Policy->GetPolicy());
        if (!Policy->SetPolicy(EUrbanViewPolicy::FirstPersonOnly))
        {
            PreviousPolicies.Remove(PawnKey);
            return false;
        }
        return true;
    }

    if (StationMode == EUrbanCharacterCameraTestStationMode::DeathReset)
    {
        ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
        if (!Health || Health->IsDeadOrDying())
        {
            return false;
        }

        Health->DamageSelfDestruct();
        return true;
    }

    const FName TagName = GetRestrictionTagName();
    const FGameplayTag RestrictionTag = FGameplayTag::RequestGameplayTag(TagName, false);
    ULyraAbilitySystemComponent* AbilitySystem = Cast<ULyraAbilitySystemComponent>(
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn, true));
    if (!AbilitySystem || !RestrictionTag.IsValid())
    {
        return false;
    }

    AbilitySystem->AddLooseGameplayTag(RestrictionTag);
    return true;
}

bool AUrbanCharacterCameraTestStation::RemoveFromPawn(APawn* Pawn)
{
    if (!IsValid(Pawn) || !HasAuthority())
    {
        return false;
    }

    if (StationMode == EUrbanCharacterCameraTestStationMode::ForcedFirstPerson)
    {
        const TWeakObjectPtr<APawn> PawnKey(Pawn);
        const EUrbanViewPolicy* PreviousPolicy = PreviousPolicies.Find(PawnKey);
        UUrbanViewPolicyComponent* Policy =
            Pawn->FindComponentByClass<UUrbanViewPolicyComponent>();
        if (!Policy || !PreviousPolicy)
        {
            return false;
        }

        const EUrbanViewPolicy PolicyToRestore = *PreviousPolicy;
        PreviousPolicies.Remove(PawnKey);
        return Policy->SetPolicy(PolicyToRestore);
    }

    if (StationMode == EUrbanCharacterCameraTestStationMode::DeathReset)
    {
        return true;
    }

    const FGameplayTag RestrictionTag = FGameplayTag::RequestGameplayTag(
        GetRestrictionTagName(),
        false);
    ULyraAbilitySystemComponent* AbilitySystem = Cast<ULyraAbilitySystemComponent>(
        UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn, true));
    if (!AbilitySystem || !RestrictionTag.IsValid())
    {
        return false;
    }

    AbilitySystem->RemoveLooseGameplayTag(RestrictionTag);
    return true;
}

void AUrbanCharacterCameraTestStation::HandleTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComponent;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || ActivePawns.Contains(Pawn))
    {
        return;
    }

    if (ApplyToPawn(Pawn))
    {
        ActivePawns.Add(Pawn);
    }
}

void AUrbanCharacterCameraTestStation::HandleTriggerEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    (void)OverlappedComponent;
    (void)OtherComponent;
    (void)OtherBodyIndex;

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !ActivePawns.Remove(Pawn))
    {
        return;
    }

    RemoveFromPawn(Pawn);
}

void AUrbanCharacterCameraTestStation::RefreshVisuals()
{
    if (!Trigger || !FloorMarker || !StationLabel)
    {
        return;
    }

    const FVector SafeExtent(
        FMath::Max(TriggerExtent.X, 25.0),
        FMath::Max(TriggerExtent.Y, 25.0),
        FMath::Max(TriggerExtent.Z, 25.0));
    Trigger->SetBoxExtent(SafeExtent);

    FloorMarker->SetRelativeLocation(FVector(0.0, 0.0, -SafeExtent.Z + 4.0));
    FloorMarker->SetRelativeScale3D(FVector(
        SafeExtent.X / 50.0,
        SafeExtent.Y / 50.0,
        0.08));

    StationLabel->SetRelativeLocation(FVector(0.0, 0.0, SafeExtent.Z + 45.0));
    StationLabel->SetText(FText::FromString(GetStationLabel(StationMode)));
    StationLabel->SetTextRenderColor(GetStationColor(StationMode));
}

FString AUrbanCharacterCameraTestStation::GetStationLabel(
    const EUrbanCharacterCameraTestStationMode Mode)
{
    switch (Mode)
    {
    case EUrbanCharacterCameraTestStationMode::AimingRestriction:
        return TEXT("AIMING - V BLOCKED");
    case EUrbanCharacterCameraTestStationMode::SprintingRestriction:
        return TEXT("SPRINTING - V BLOCKED");
    case EUrbanCharacterCameraTestStationMode::TraversalRestriction:
        return TEXT("TRAVERSAL - V BLOCKED");
    case EUrbanCharacterCameraTestStationMode::DownedRestriction:
        return TEXT("DOWNED - V BLOCKED");
    case EUrbanCharacterCameraTestStationMode::ForcedFirstPerson:
        return TEXT("FORCED FIRST PERSON");
    case EUrbanCharacterCameraTestStationMode::DeathReset:
        return TEXT("DEATH / RESPAWN");
    default:
        return TEXT("CHARACTER CAMERA TEST");
    }
}

FColor AUrbanCharacterCameraTestStation::GetStationColor(
    const EUrbanCharacterCameraTestStationMode Mode)
{
    switch (Mode)
    {
    case EUrbanCharacterCameraTestStationMode::ForcedFirstPerson:
        return FColor::Cyan;
    case EUrbanCharacterCameraTestStationMode::DeathReset:
        return FColor::Red;
    case EUrbanCharacterCameraTestStationMode::TraversalRestriction:
        return FColor::Yellow;
    default:
        return FColor::Green;
    }
}