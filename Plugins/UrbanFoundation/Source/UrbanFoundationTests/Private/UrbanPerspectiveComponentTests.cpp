#include "CoreMinimal.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/UrbanCharacterStateComponent.h"
#include "Character/UrbanPerspectiveComponent.h"
#include "Character/UrbanViewPolicyComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UrbanPerspectiveComponentTests
{
    APawn* CreateTransientPawn()
    {
        return NewObject<APawn>(GetTransientPackage(), NAME_None, RF_Transient);
    }

    void AttachComponent(APawn* Pawn, UActorComponent* Component)
    {
        Pawn->AddInstanceComponent(Component);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCharacterStateComponentTest,
    "UrbanSpear.CharacterCamera.Components.CharacterState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCharacterStateComponentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveComponentTests;

    APawn* Pawn = CreateTransientPawn();
    ULyraAbilitySystemComponent* AbilitySystem = NewObject<ULyraAbilitySystemComponent>(Pawn);
    UUrbanCharacterStateComponent* StateComponent = NewObject<UUrbanCharacterStateComponent>(Pawn);
    AttachComponent(Pawn, AbilitySystem);
    AttachComponent(Pawn, StateComponent);

    const FUrbanCharacterViewState DefaultState = StateComponent->GetViewState();
    TestTrue(TEXT("default state is initialized"), DefaultState.bInitialized);
    TestFalse(TEXT("default state is not aiming"), DefaultState.bAiming);
    TestFalse(TEXT("default state is not sprinting"), DefaultState.bSprinting);
    TestFalse(TEXT("default state is not traversing"), DefaultState.bTraversal);
    TestFalse(TEXT("default state is not downed"), DefaultState.bDowned);
    TestFalse(TEXT("default state is not dead"), DefaultState.bDead);

    struct FTagExpectation
    {
        const TCHAR* TagName;
        bool FUrbanCharacterViewState::* Field;
        const TCHAR* Message;
    };

    const FTagExpectation Expectations[] = {
        {TEXT("Status.Aiming"), &FUrbanCharacterViewState::bAiming, TEXT("aiming tag maps to aiming state")},
        {TEXT("Status.Sprinting"), &FUrbanCharacterViewState::bSprinting, TEXT("sprinting tag maps to sprinting state")},
        {TEXT("Status.Traversal"), &FUrbanCharacterViewState::bTraversal, TEXT("traversal tag maps to traversal state")},
        {TEXT("Status.Downed"), &FUrbanCharacterViewState::bDowned, TEXT("downed tag maps to downed state")},
        {TEXT("Status.Death"), &FUrbanCharacterViewState::bDead, TEXT("death tag maps to dead state")},
    };

    for (const FTagExpectation& Expectation : Expectations)
    {
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Expectation.TagName));
        AbilitySystem->AddLooseGameplayTag(Tag);
        const FUrbanCharacterViewState TaggedState = StateComponent->GetViewState();
        TestTrue(Expectation.Message, TaggedState.*(Expectation.Field));
        AbilitySystem->RemoveLooseGameplayTag(Tag);
        const FUrbanCharacterViewState ClearedState = StateComponent->GetViewState();
        TestFalse(TEXT("removing a restriction tag clears its state"), ClearedState.*(Expectation.Field));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanViewPolicyComponentTest,
    "UrbanSpear.CharacterCamera.Components.ViewPolicyAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanViewPolicyComponentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveComponentTests;

    APawn* Pawn = CreateTransientPawn();
    Pawn->SetRole(ROLE_SimulatedProxy);
    UUrbanViewPolicyComponent* PolicyComponent = NewObject<UUrbanViewPolicyComponent>(Pawn);
    AttachComponent(Pawn, PolicyComponent);

    TestEqual(TEXT("default policy allows free choice"), PolicyComponent->GetPolicy(), EUrbanViewPolicy::FreeChoice);
    TestFalse(TEXT("non-authority cannot mutate policy"), PolicyComponent->SetPolicy(EUrbanViewPolicy::FirstPersonOnly));
    TestEqual(TEXT("rejected mutation preserves policy"), PolicyComponent->GetPolicy(), EUrbanViewPolicy::FreeChoice);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectiveStateMachineTest,
    "UrbanSpear.CharacterCamera.StateMachine.PerspectiveLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectiveStateMachineTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveComponentTests;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    APawn* Pawn = World->SpawnActor<APawn>();
    ULyraAbilitySystemComponent* AbilitySystem = NewObject<ULyraAbilitySystemComponent>(Pawn);
    UUrbanCharacterStateComponent* StateComponent = NewObject<UUrbanCharacterStateComponent>(Pawn);
    UUrbanViewPolicyComponent* PolicyComponent = NewObject<UUrbanViewPolicyComponent>(Pawn);
    UUrbanPerspectiveComponent* PerspectiveComponent = NewObject<UUrbanPerspectiveComponent>(Pawn);

    UActorComponent* Components[] = {
        AbilitySystem,
        StateComponent,
        PolicyComponent,
        PerspectiveComponent,
    };
    for (UActorComponent* Component : Components)
    {
        AttachComponent(Pawn, Component);
        Component->RegisterComponent();
    }

    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    Pawn->DispatchBeginPlay();
    TestTrue(TEXT("perspective component begins play with its owner"), PerspectiveComponent->HasBegunPlay());

    TestEqual(TEXT("accepted perspective defaults to first person"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::FirstPerson);
    TestEqual(TEXT("preferred perspective defaults to first person"), PerspectiveComponent->GetPreferredPerspective(), EUrbanPerspective::FirstPerson);

    PerspectiveComponent->ServerRequestPerspective(EUrbanPerspective::ThirdPersonRight);
    TestEqual(TEXT("legal request changes accepted perspective"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonRight);
    TestEqual(TEXT("legal request saves preferred perspective"), PerspectiveComponent->GetPreferredPerspective(), EUrbanPerspective::ThirdPersonRight);

    TestTrue(TEXT("authority can force first person"), PolicyComponent->SetPolicy(EUrbanViewPolicy::FirstPersonOnly));
    TestEqual(TEXT("forced policy changes accepted perspective"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ForcedFirstPerson);
    TestEqual(TEXT("forced policy preserves player preference"), PerspectiveComponent->GetPreferredPerspective(), EUrbanPerspective::ThirdPersonRight);

    TestTrue(TEXT("authority can restore free choice"), PolicyComponent->SetPolicy(EUrbanViewPolicy::FreeChoice));
    TestEqual(TEXT("ending force restores saved preference"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonRight);

    const FGameplayTag AimingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Status.Aiming")));
    AbilitySystem->AddLooseGameplayTag(AimingTag);
    PerspectiveComponent->ServerRequestPerspective(EUrbanPerspective::FirstPerson);
    TestEqual(TEXT("aiming rejects a perspective request"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonRight);
    AbilitySystem->RemoveLooseGameplayTag(AimingTag);
    TestEqual(TEXT("rejected request is not replayed after aiming clears"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonRight);

    PerspectiveComponent->RequestToggleShoulder();
    TestEqual(TEXT("third person can switch shoulders"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonLeft);
    PerspectiveComponent->ServerRequestPerspective(EUrbanPerspective::FirstPerson);
    PerspectiveComponent->RequestToggleShoulder();
    TestEqual(TEXT("first person ignores shoulder requests"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::FirstPerson);

    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);

    return true;
}

#endif