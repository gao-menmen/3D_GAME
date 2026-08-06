#include "CoreMinimal.h"
#include "Character/UrbanPerspectiveTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectiveRulesTest,
    "UrbanSpear.CharacterCamera.Rules.PerspectiveDecisions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectiveRulesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FUrbanCharacterViewState ReadyState;
    const FUrbanPerspectiveDecision FreeChoice = FUrbanPerspectiveRules::Evaluate(
        EUrbanPerspective::FirstPerson,
        EUrbanPerspective::ThirdPersonRight,
        EUrbanViewPolicy::FreeChoice,
        ReadyState);
    TestTrue(TEXT("free choice request is accepted"), FreeChoice.bAccepted);
    TestEqual(TEXT("free choice accepts third person"), FreeChoice.AcceptedPerspective, EUrbanPerspective::ThirdPersonRight);
    TestEqual(TEXT("accepted request has no block reason"), FreeChoice.Reason, EUrbanPerspectiveBlockReason::None);

    FUrbanCharacterViewState AimingState;
    AimingState.bAiming = true;
    const FUrbanPerspectiveDecision Aiming = FUrbanPerspectiveRules::Evaluate(
        EUrbanPerspective::FirstPerson,
        EUrbanPerspective::ThirdPersonRight,
        EUrbanViewPolicy::FreeChoice,
        AimingState);
    TestFalse(TEXT("aiming rejects change"), Aiming.bAccepted);
    TestEqual(TEXT("aiming keeps current perspective"), Aiming.AcceptedPerspective, EUrbanPerspective::FirstPerson);
    TestEqual(TEXT("aiming reports its reason"), Aiming.Reason, EUrbanPerspectiveBlockReason::Aiming);

    const FUrbanPerspectiveDecision Forced = FUrbanPerspectiveRules::Evaluate(
        EUrbanPerspective::ThirdPersonRight,
        EUrbanPerspective::ThirdPersonLeft,
        EUrbanViewPolicy::FirstPersonOnly,
        ReadyState);
    TestFalse(TEXT("forced policy rejects player request"), Forced.bAccepted);
    TestEqual(TEXT("server policy forces first person"), Forced.AcceptedPerspective, EUrbanPerspective::ForcedFirstPerson);
    TestEqual(TEXT("forced policy reports policy reason"), Forced.Reason, EUrbanPerspectiveBlockReason::Policy);

    FUrbanCharacterViewState DeadState;
    DeadState.bDead = true;
    const FUrbanPerspectiveDecision Dead = FUrbanPerspectiveRules::Evaluate(
        EUrbanPerspective::ThirdPersonRight,
        EUrbanPerspective::FirstPerson,
        EUrbanViewPolicy::FreeChoice,
        DeadState);
    TestFalse(TEXT("death rejects change"), Dead.bAccepted);
    TestEqual(TEXT("death reports its reason"), Dead.Reason, EUrbanPerspectiveBlockReason::Dead);

    return true;
}

#endif
