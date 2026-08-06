#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectiveInputAssetsTest,
    "UrbanSpear.CharacterCamera.Input.Assets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectiveInputAssetsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const UInputAction* PerspectiveAction = LoadObject<UInputAction>(
        nullptr,
        TEXT("/UrbanFoundation/Input/Actions/IA_UrbanTogglePerspective.IA_UrbanTogglePerspective"));
    const UInputAction* ShoulderAction = LoadObject<UInputAction>(
        nullptr,
        TEXT("/UrbanFoundation/Input/Actions/IA_UrbanToggleShoulder.IA_UrbanToggleShoulder"));
    const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(
        nullptr,
        TEXT("/UrbanFoundation/Input/Mappings/IMC_UrbanPerspective_KBM.IMC_UrbanPerspective_KBM"));

    TestNotNull(TEXT("perspective input action exists"), PerspectiveAction);
    TestNotNull(TEXT("shoulder input action exists"), ShoulderAction);
    TestNotNull(TEXT("perspective keyboard mapping context exists"), MappingContext);

    if (PerspectiveAction)
    {
        TestEqual(
            TEXT("perspective action is boolean"),
            PerspectiveAction->ValueType,
            EInputActionValueType::Boolean);
        TestTrue(
            TEXT("perspective action triggers once per press"),
            PerspectiveAction->Triggers.ContainsByPredicate(
                [](const UInputTrigger* Trigger) { return Trigger && Trigger->IsA<UInputTriggerPressed>(); }));
    }
    if (ShoulderAction)
    {
        TestEqual(
            TEXT("shoulder action is boolean"),
            ShoulderAction->ValueType,
            EInputActionValueType::Boolean);
        TestTrue(
            TEXT("shoulder action triggers once per press"),
            ShoulderAction->Triggers.ContainsByPredicate(
                [](const UInputTrigger* Trigger) { return Trigger && Trigger->IsA<UInputTriggerPressed>(); }));
    }

    if (PerspectiveAction == nullptr || ShoulderAction == nullptr || MappingContext == nullptr)
    {
        return false;
    }

    bool bPerspectiveUsesV = false;
    bool bShoulderUsesQ = false;
    MappingContext->ForEachKeyMapping(
        [&bPerspectiveUsesV, &bShoulderUsesQ, PerspectiveAction, ShoulderAction](const FEnhancedActionKeyMapping& Mapping)
        {
            bPerspectiveUsesV |= Mapping.Action == PerspectiveAction && Mapping.Key == EKeys::V;
            bShoulderUsesQ |= Mapping.Action == ShoulderAction && Mapping.Key == EKeys::Q;
        });

    TestTrue(TEXT("V toggles first and third person"), bPerspectiveUsesV);
    TestTrue(TEXT("Q toggles the active third-person shoulder"), bShoulderUsesQ);

    return true;
}

#endif
