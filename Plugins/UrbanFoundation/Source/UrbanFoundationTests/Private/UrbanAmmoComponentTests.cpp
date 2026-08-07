#include "Combat/UrbanAmmoComponent.h"
#include "Components/ActorComponent.h"
#include "Misc/AutomationTest.h"
#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanAmmoComponentContractTest,
    "UrbanSpear.Combat.Ammo.ComponentContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanAmmoComponentContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    static_assert(std::is_base_of_v<UActorComponent, UUrbanAmmoComponent>);

    UUrbanAmmoComponent* Component = NewObject<UUrbanAmmoComponent>();
    TestTrue(TEXT("ammo component replicates by default"), Component->GetIsReplicated());

    const UFunction* ConfigureFunction = UUrbanAmmoComponent::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UUrbanAmmoComponent, ConfigureAuthoritative));
    const UFunction* FireFunction = UUrbanAmmoComponent::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UUrbanAmmoComponent, TryConsumeRoundAuthoritative));
    const UFunction* ReloadFunction = UUrbanAmmoComponent::StaticClass()->FindFunctionByName(
        GET_FUNCTION_NAME_CHECKED(UUrbanAmmoComponent, BeginReloadAuthoritative));

    TestTrue(TEXT("configure is marked authority only"), ConfigureFunction != nullptr && ConfigureFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
    TestTrue(TEXT("fire is marked authority only"), FireFunction != nullptr && FireFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
    TestTrue(TEXT("reload is marked authority only"), ReloadFunction != nullptr && ReloadFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));

    TestFalse(TEXT("unowned component rejects authoritative configure"),
        Component->ConfigureAuthoritative(FUrbanWeaponTuning::MakeDefaults(EUrbanWeaponArchetype::AssaultRifle)));
    TestFalse(TEXT("unowned component rejects fire"), Component->TryConsumeRoundAuthoritative());

    return true;
}

#endif
