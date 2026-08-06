#include "CoreMinimal.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/UrbanCharacterStateComponent.h"
#include "Character/UrbanPerspectiveComponent.h"
#include "Character/UrbanPerspectivePreferenceComponent.h"
#include "Character/UrbanViewPolicyComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UrbanPerspectiveAuthorityTests
{
    struct FPerspectivePawnFixture
    {
        APawn* Pawn = nullptr;
        ULyraAbilitySystemComponent* AbilitySystem = nullptr;
        UUrbanCharacterStateComponent* State = nullptr;
        UUrbanViewPolicyComponent* Policy = nullptr;
        UUrbanPerspectiveComponent* Perspective = nullptr;
    };

    FPerspectivePawnFixture AddPerspectivePawn(UWorld* World)
    {
        FPerspectivePawnFixture Fixture;
        Fixture.Pawn = World->SpawnActorDeferred<APawn>(
            APawn::StaticClass(),
            FTransform::Identity);
        Fixture.AbilitySystem = NewObject<ULyraAbilitySystemComponent>(Fixture.Pawn);
        Fixture.State = NewObject<UUrbanCharacterStateComponent>(Fixture.Pawn);
        Fixture.Policy = NewObject<UUrbanViewPolicyComponent>(Fixture.Pawn);
        Fixture.Perspective = NewObject<UUrbanPerspectiveComponent>(Fixture.Pawn);

        UActorComponent* Components[] = {
            Fixture.AbilitySystem,
            Fixture.State,
            Fixture.Policy,
            Fixture.Perspective,
        };
        for (UActorComponent* Component : Components)
        {
            Fixture.Pawn->AddInstanceComponent(Component);
            Component->RegisterComponent();
        }

        Fixture.Pawn->FinishSpawning(FTransform::Identity);
        if (World->AreActorsInitialized() && !Fixture.Pawn->HasActorBegunPlay())
        {
            Fixture.Pawn->DispatchBeginPlay();
        }
        return Fixture;
    }

    UWorld* CreateGameWorld()
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        WorldContext.SetCurrentWorld(World);
        return World;
    }

    void StartGameWorld(UWorld* World)
    {
        World->InitializeActorsForPlay(FURL());
        World->BeginPlay();
    }

    void SetPolicyWithoutNotification(
        UUrbanViewPolicyComponent* Policy,
        const EUrbanViewPolicy NewPolicy)
    {
        FEnumProperty* PolicyProperty = CastFieldChecked<FEnumProperty>(
            Policy->GetClass()->FindPropertyByName(TEXT("Policy")));
        void* ValueAddress = PolicyProperty->ContainerPtrToValuePtr<void>(Policy);
        PolicyProperty->GetUnderlyingProperty()->SetIntPropertyValue(
            ValueAddress,
            static_cast<uint64>(NewPolicy));
    }

    void NotifyPerspectivePolicyChanged(
        UUrbanPerspectiveComponent* Perspective,
        const EUrbanViewPolicy NewPolicy)
    {
        UFunction* Handler = Perspective->FindFunctionChecked(TEXT("HandlePolicyChanged"));
        struct FPolicyChangedParameters
        {
            EUrbanViewPolicy Policy;
        } Parameters{NewPolicy};
        Perspective->ProcessEvent(Handler, &Parameters);
    }

    void DestroyGameWorld(UWorld* World)
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectiveServerTransitionLockTest,
    "UrbanSpear.CharacterCamera.Network.ServerTransitionLock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectiveServerTransitionLockTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveAuthorityTests;

    UWorld* World = CreateGameWorld();
    const FPerspectivePawnFixture Fixture = AddPerspectivePawn(World);
    StartGameWorld(World);

    Fixture.Perspective->ServerRequestPerspective(EUrbanPerspective::ThirdPersonRight);
    Fixture.Perspective->ServerRequestPerspective(EUrbanPerspective::ThirdPersonLeft);
    TestEqual(
        TEXT("the server rejects a second request while its authoritative transition window is active"),
        Fixture.Perspective->GetAcceptedPerspective(),
        EUrbanPerspective::ThirdPersonRight);

    World->Tick(LEVELTICK_All, 0.25f);
    Fixture.Perspective->ServerRequestPerspective(EUrbanPerspective::ThirdPersonLeft);
    TestEqual(
        TEXT("the server accepts a new request after the transition window expires"),
        Fixture.Perspective->GetAcceptedPerspective(),
        EUrbanPerspective::ThirdPersonLeft);

    Fixture.Pawn->SetRole(ROLE_SimulatedProxy);
    SetPolicyWithoutNotification(Fixture.Policy, EUrbanViewPolicy::FirstPersonOnly);
    TestEqual(
        TEXT("the simulated replicated policy value is visible before notification"),
        Fixture.Policy->GetPolicy(),
        EUrbanViewPolicy::FirstPersonOnly);
    TestFalse(
        TEXT("the replicated pawn is no longer authoritative"),
        Fixture.Pawn->HasAuthority());
    NotifyPerspectivePolicyChanged(Fixture.Perspective, EUrbanViewPolicy::FirstPersonOnly);
    TestEqual(
        TEXT("a replicated policy notification cannot mutate the server-owned perspective on a client"),
        Fixture.Perspective->GetAcceptedPerspective(),
        EUrbanPerspective::ThirdPersonLeft);

    DestroyGameWorld(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectivePreferenceRespawnTest,
    "UrbanSpear.CharacterCamera.StateMachine.PreferenceAndRespawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectivePreferenceRespawnTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveAuthorityTests;

    UWorld* World = CreateGameWorld();
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    const FPerspectivePawnFixture FirstPawn = AddPerspectivePawn(World);
    Controller->Possess(FirstPawn.Pawn);
    StartGameWorld(World);

    FirstPawn.Perspective->ServerRequestPerspective(EUrbanPerspective::ThirdPersonLeft);
    World->Tick(LEVELTICK_All, 0.25f);
    FirstPawn.Perspective->ServerRequestPerspective(EUrbanPerspective::FirstPerson);
    World->Tick(LEVELTICK_All, 0.25f);
    FirstPawn.Perspective->RequestTogglePerspective();
    TestEqual(
        TEXT("returning to third person preserves the last selected left shoulder"),
        FirstPawn.Perspective->GetAcceptedPerspective(),
        EUrbanPerspective::ThirdPersonLeft);

    World->Tick(LEVELTICK_All, 0.25f);
    FirstPawn.Perspective->ServerRequestPerspective(EUrbanPerspective::FirstPerson);
    World->Tick(LEVELTICK_All, 0.25f);
    FirstPawn.Perspective->ServerRequestPerspective(EUrbanPerspective::ThirdPersonLeft);
    World->Tick(LEVELTICK_All, 0.25f);

    UUrbanPerspectivePreferenceComponent* StoredPreference =
        Controller->FindComponentByClass<UUrbanPerspectivePreferenceComponent>();
    TestNotNull(TEXT("the controller owns a persistent perspective preference component"), StoredPreference);
    if (StoredPreference)
    {
        TestEqual(
            TEXT("the controller stores the last legal free-choice perspective"),
            StoredPreference->GetPreferredPerspective(),
            EUrbanPerspective::ThirdPersonLeft);
        TestEqual(
            TEXT("the controller stores the last third-person shoulder"),
            StoredPreference->GetPreferredThirdPersonPerspective(),
            EUrbanPerspective::ThirdPersonLeft);
    }

    Controller->UnPossess();
    FirstPawn.Pawn->Destroy();
    World->Tick(LEVELTICK_All, 0.01f);

    const FPerspectivePawnFixture RespawnedPawn = AddPerspectivePawn(World);
    TestTrue(TEXT("the replacement perspective component has begun play"), RespawnedPawn.Perspective->HasBegunPlay());
    Controller->Possess(RespawnedPawn.Pawn);
    World->Tick(LEVELTICK_All, 0.01f);
    TestTrue(TEXT("the controller possesses the replacement pawn"), Controller->GetPawn() == RespawnedPawn.Pawn);
    TestEqual(
        TEXT("the replacement pawn receives the controller-owned preference component"),
        Controller->FindComponentByClass<UUrbanPerspectivePreferenceComponent>(),
        StoredPreference);

    TestEqual(
        TEXT("a replacement pawn restores the controller-owned legal perspective preference"),
        RespawnedPawn.Perspective->GetAcceptedPerspective(),
        EUrbanPerspective::ThirdPersonLeft);
    TestEqual(
        TEXT("a replacement pawn restores the controller-owned third-person shoulder preference"),
        RespawnedPawn.Perspective->GetPreferredPerspective(),
        EUrbanPerspective::ThirdPersonLeft);

    DestroyGameWorld(World);
    return true;
}

#endif

