#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/UrbanCharacterStateComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UrbanCharacterAbilityStateTests
{
    FGameplayAbilitySpec* MarkAbilityActive(
        FAutomationTestBase& Test,
        ULyraAbilitySystemComponent* AbilitySystem,
        const TCHAR* AbilityClassPath)
    {
        UClass* AbilityClass = LoadClass<UGameplayAbility>(nullptr, AbilityClassPath);
        Test.TestNotNull(TEXT("the ShooterCore gameplay ability class loads"), AbilityClass);
        if (!AbilityClass)
        {
            return nullptr;
        }

        const FGameplayAbilitySpecHandle Handle = AbilitySystem->GiveAbility(
            FGameplayAbilitySpec(AbilityClass, 1));
        FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle);
        Test.TestNotNull(TEXT("the real gameplay ability is granted to the ability system"), Spec);
        if (!Spec)
        {
            return nullptr;
        }

        Spec->ActiveCount = 1;
        Test.TestTrue(TEXT("the granted gameplay ability spec is active"), Spec->IsActive());
        return Spec;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCharacterAbilityStateTest,
    "UrbanSpear.CharacterCamera.Components.RealAbilityState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCharacterAbilityStateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanCharacterAbilityStateTests;

    APawn* Pawn = NewObject<APawn>(GetTransientPackage(), NAME_None, RF_Transient);
    ULyraAbilitySystemComponent* AbilitySystem = NewObject<ULyraAbilitySystemComponent>(Pawn);
    UUrbanCharacterStateComponent* StateComponent = NewObject<UUrbanCharacterStateComponent>(Pawn);
    Pawn->AddInstanceComponent(AbilitySystem);
    Pawn->AddInstanceComponent(StateComponent);

    if (FGameplayAbilitySpec* AdsSpec = MarkAbilityActive(
            *this,
            AbilitySystem,
            TEXT("/ShooterCore/Input/Abilities/GA_ADS.GA_ADS_C")))
    {
        TestTrue(
            TEXT("an active real ShooterCore ADS ability blocks perspective switching as aiming"),
            StateComponent->GetViewState().bAiming);
        AdsSpec->ActiveCount = 0;
    }

    AbilitySystem->ClearAllAbilities();

    if (FGameplayAbilitySpec* DashSpec = MarkAbilityActive(
            *this,
            AbilitySystem,
            TEXT("/ShooterCore/Game/Dash/GA_Hero_Dash.GA_Hero_Dash_C")))
    {
        TestTrue(
            TEXT("an active real ShooterCore dash ability blocks perspective switching as sprinting"),
            StateComponent->GetViewState().bSprinting);
        DashSpec->ActiveCount = 0;
    }

    AbilitySystem->ClearAllAbilities();
    return true;
}

#endif