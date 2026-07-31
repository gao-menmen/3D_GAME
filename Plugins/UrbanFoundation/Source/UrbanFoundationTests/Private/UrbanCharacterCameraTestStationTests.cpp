#include "CoreMinimal.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/UrbanViewPolicyComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Testing/UrbanCharacterCameraTestStation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCharacterCameraTestStationTest,
    "UrbanSpear.CharacterCamera.Integration.TestStation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCharacterCameraTestStationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);

    APawn* Pawn = World->SpawnActor<APawn>();
    ULyraAbilitySystemComponent* AbilitySystem = NewObject<ULyraAbilitySystemComponent>(Pawn);
    UUrbanViewPolicyComponent* ViewPolicy = NewObject<UUrbanViewPolicyComponent>(Pawn);
    Pawn->AddInstanceComponent(AbilitySystem);
    Pawn->AddInstanceComponent(ViewPolicy);
    AbilitySystem->RegisterComponent();
    ViewPolicy->RegisterComponent();

    AUrbanCharacterCameraTestStation* Station =
        World->SpawnActor<AUrbanCharacterCameraTestStation>();
    Station->SetTriggerExtent(FVector(300.0, 200.0, 140.0));
    TestEqual(
        TEXT("station extent setter refreshes the overlap volume"),
        Station->Trigger->GetUnscaledBoxExtent(),
        FVector(300.0, 200.0, 140.0));

    struct FRestrictionExpectation
    {
        EUrbanCharacterCameraTestStationMode Mode;
        const TCHAR* TagName;
    };

    const FRestrictionExpectation Expectations[] = {
        {EUrbanCharacterCameraTestStationMode::AimingRestriction, TEXT("Status.Aiming")},
        {EUrbanCharacterCameraTestStationMode::SprintingRestriction, TEXT("Status.Sprinting")},
        {EUrbanCharacterCameraTestStationMode::TraversalRestriction, TEXT("Status.Traversal")},
        {EUrbanCharacterCameraTestStationMode::DownedRestriction, TEXT("Status.Downed")},
    };

    for (const FRestrictionExpectation& Expectation : Expectations)
    {
        Station->SetStationMode(Expectation.Mode);
        TestEqual(
            TEXT("station mode maps to the expected restriction tag"),
            Station->GetRestrictionTagName(),
            FName(Expectation.TagName));

        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Expectation.TagName));
        TestTrue(TEXT("restriction station applies its gameplay tag"), Station->ApplyToPawn(Pawn));
        TestTrue(TEXT("pawn owns the station gameplay tag while inside"), AbilitySystem->HasMatchingGameplayTag(Tag));
        TestTrue(TEXT("restriction station removes its gameplay tag"), Station->RemoveFromPawn(Pawn));
        TestFalse(TEXT("pawn no longer owns the station gameplay tag after exit"), AbilitySystem->HasMatchingGameplayTag(Tag));
    }

    Station->SetStationMode(EUrbanCharacterCameraTestStationMode::ForcedFirstPerson);
    TestTrue(TEXT("forced-first-person station applies its policy"), Station->ApplyToPawn(Pawn));
    TestEqual(TEXT("forced station changes policy"), ViewPolicy->GetPolicy(), EUrbanViewPolicy::FirstPersonOnly);
    TestTrue(TEXT("forced-first-person station restores its policy"), Station->RemoveFromPawn(Pawn));
    TestEqual(TEXT("leaving forced station restores free choice"), ViewPolicy->GetPolicy(), EUrbanViewPolicy::FreeChoice);

    Station->SetStationMode(EUrbanCharacterCameraTestStationMode::DeathReset);
    TestEqual(TEXT("death station does not map to a loose restriction tag"), Station->GetRestrictionTagName(), NAME_None);

    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);

    return true;
}

#endif
