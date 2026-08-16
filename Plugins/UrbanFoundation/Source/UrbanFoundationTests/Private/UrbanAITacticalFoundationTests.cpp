#include "AI/UrbanAITypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanEnemyArchetypeDefaultsTest,
    "UrbanSpear.AI.Archetypes.FourEnemyRoles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanEnemyArchetypeDefaultsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FUrbanEnemyArchetypeTuning Rifleman = FUrbanEnemyArchetypeTuning::MakeDefaults(EUrbanEnemyArchetype::Rifleman);
    const FUrbanEnemyArchetypeTuning Assault = FUrbanEnemyArchetypeTuning::MakeDefaults(EUrbanEnemyArchetype::Assault);
    const FUrbanEnemyArchetypeTuning Marksman = FUrbanEnemyArchetypeTuning::MakeDefaults(EUrbanEnemyArchetype::Marksman);
    const FUrbanEnemyArchetypeTuning Drone = FUrbanEnemyArchetypeTuning::MakeDefaults(EUrbanEnemyArchetype::Drone);

    TestTrue(TEXT("assault closes to the shortest range"),
        Assault.PreferredEngagementRangeMeters < Rifleman.PreferredEngagementRangeMeters);
    TestTrue(TEXT("marksman holds the longest range"),
        Marksman.PreferredEngagementRangeMeters > Drone.PreferredEngagementRangeMeters
        && Drone.PreferredEngagementRangeMeters > Rifleman.PreferredEngagementRangeMeters);
    TestTrue(TEXT("assault favors flanking"), Assault.FlankPreference > Rifleman.FlankPreference);
    TestTrue(TEXT("marksman favors cover"), Marksman.CoverPreference > 0.9f);
    TestTrue(TEXT("assault can use grenades"), Assault.bCanUseGrenades);
    TestTrue(TEXT("drone is aerial"), Drone.bIsAerial);
    TestFalse(TEXT("ground roles remain non-aerial"), Rifleman.bIsAerial || Assault.bIsAerial || Marksman.bIsAerial);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanEnemyArchetypeSanitizeTest,
    "UrbanSpear.AI.Archetypes.Sanitize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanEnemyArchetypeSanitizeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanEnemyArchetypeTuning Tuning;
    Tuning.PreferredEngagementRangeMeters = -10.0f;
    Tuning.ReactionTimeSeconds = 0.0f;
    Tuning.AimSpreadDegrees = 100.0f;
    Tuning.CoverPreference = 4.0f;
    Tuning.FlankPreference = -4.0f;
    Tuning.Sanitize();

    TestEqual(TEXT("engagement range has a safe minimum"), Tuning.PreferredEngagementRangeMeters, 2.0f);
    TestEqual(TEXT("reaction time has a safe minimum"), Tuning.ReactionTimeSeconds, 0.1f);
    TestEqual(TEXT("aim spread is bounded"), Tuning.AimSpreadDegrees, 15.0f);
    TestEqual(TEXT("cover preference is normalized"), Tuning.CoverPreference, 1.0f);
    TestEqual(TEXT("flank preference is normalized"), Tuning.FlankPreference, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanAIPerceptionStateTest,
    "UrbanSpear.AI.Decision.PerceptionDrivenStates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanAIPerceptionStateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanAIDecisionContext Context;
    TestEqual(TEXT("no knowledge returns to patrol or guard"),
        UUrbanAIDecisionLibrary::ResolveBehaviorState(Context), EUrbanAIBehaviorState::PatrolOrGuard);

    Context.bHasRecentStimulus = true;
    TestEqual(TEXT("recent clue is investigated"),
        UUrbanAIDecisionLibrary::ResolveBehaviorState(Context), EUrbanAIBehaviorState::Investigate);

    Context.bReachedLastKnownLocation = true;
    TestEqual(TEXT("last-known location transitions to search"),
        UUrbanAIDecisionLibrary::ResolveBehaviorState(Context), EUrbanAIBehaviorState::Search);

    Context.bHasConfirmedTarget = true;
    TestEqual(TEXT("confirmed target enters cover combat"),
        UUrbanAIDecisionLibrary::ResolveBehaviorState(Context), EUrbanAIBehaviorState::CoverCombat);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanAITacticalGateTest,
    "UrbanSpear.AI.Decision.TacticalGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanAITacticalGateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanAIDecisionContext Context;
    Context.bHasConfirmedTarget = true;
    Context.TimeInStateSeconds = 15.0f;

    TestEqual(TEXT("time alone cannot authorize tactics"),
        UUrbanAIDecisionLibrary::ResolveBehaviorState(Context), EUrbanAIBehaviorState::CoverCombat);

    Context.bHasValidFlankRoute = true;
    TestEqual(TEXT("navigation-approved route authorizes flank"),
        UUrbanAIDecisionLibrary::ResolveBehaviorState(Context), EUrbanAIBehaviorState::Flank);

    Context.bReinforcementBudgetAvailable = true;
    TestEqual(TEXT("budget-authorized reinforcement takes priority"),
        UUrbanAIDecisionLibrary::ResolveBehaviorState(Context), EUrbanAIBehaviorState::CallReinforcement);

    Context.bHasConfirmedTarget = false;
    Context.bHasRecentStimulus = false;
    TestEqual(TEXT("tactical flags never reveal an unperceived player"),
        UUrbanAIDecisionLibrary::ResolveBehaviorState(Context), EUrbanAIBehaviorState::PatrolOrGuard);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanAIReactionGateTest,
    "UrbanSpear.AI.Decision.ReactionGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanAIReactionGateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    TestFalse(TEXT("new target cannot be attacked immediately"),
        UUrbanAIDecisionLibrary::HasCompletedReaction(0.0f, 0.65f));
    TestFalse(TEXT("partial reaction delay remains gated"),
        UUrbanAIDecisionLibrary::HasCompletedReaction(0.64f, 0.65f));
    TestTrue(TEXT("elapsed reaction delay authorizes engagement"),
        UUrbanAIDecisionLibrary::HasCompletedReaction(0.65f, 0.65f));
    TestTrue(TEXT("negative configuration is safely treated as immediate"),
        UUrbanAIDecisionLibrary::HasCompletedReaction(0.0f, -1.0f));

    const FUrbanEnemyArchetypeTuning Assault =
        FUrbanEnemyArchetypeTuning::MakeDefaults(EUrbanEnemyArchetype::Assault);
    const FUrbanEnemyArchetypeTuning Marksman =
        FUrbanEnemyArchetypeTuning::MakeDefaults(EUrbanEnemyArchetype::Marksman);
    TestTrue(TEXT("assault reacts faster than marksman"),
        Assault.ReactionTimeSeconds < Marksman.ReactionTimeSeconds);
    TestTrue(TEXT("marksman has tighter aim spread than assault"),
        Marksman.AimSpreadDegrees < Assault.AimSpreadDegrees);
    return true;
}
#endif
