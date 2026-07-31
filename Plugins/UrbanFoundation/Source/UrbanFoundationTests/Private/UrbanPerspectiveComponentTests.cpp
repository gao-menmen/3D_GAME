#include "CoreMinimal.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/UrbanCharacterStateComponent.h"
#include "Character/UrbanViewPolicyComponent.h"
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

#endif