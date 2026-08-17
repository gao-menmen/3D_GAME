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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanAIFlankCandidateTest,
    "UrbanSpear.AI.Navigation.FlankCandidate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanAIFlankCandidateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FVector PawnLocation(0.0f, 0.0f, 0.0f);
    const FVector EnemyLocation(1000.0f, 0.0f, 0.0f);
    const FVector RightCandidate = UUrbanAIDecisionLibrary::BuildFlankCandidate(
        PawnLocation, EnemyLocation, 0.85f, 1);
    const FVector LeftCandidate = UUrbanAIDecisionLibrary::BuildFlankCandidate(
        PawnLocation, EnemyLocation, 0.85f, -1);

    TestTrue(TEXT("candidate stays behind the enemy front line"), RightCandidate.X < EnemyLocation.X);
    TestTrue(TEXT("positive side produces a lateral candidate"), RightCandidate.Y > 500.0f);
    TestTrue(TEXT("opposite sides mirror lateral distance"),
        FMath::IsNearlyEqual(RightCandidate.Y, -LeftCandidate.Y));
    TestEqual(TEXT("coincident locations remain stable"),
        UUrbanAIDecisionLibrary::BuildFlankCandidate(EnemyLocation, EnemyLocation, 1.0f, 1), EnemyLocation);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanAIReinforcementBudgetTest,
    "UrbanSpear.AI.Reinforcement.BudgetAndCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanAIReinforcementBudgetTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FUrbanReinforcementBudget Budget;
    Budget.TeamOneRemaining = 2;
    Budget.TeamTwoRemaining = 1;
    Budget.RequestCooldownSeconds = 25.0f;

    TestTrue(TEXT("team one initially has an authorized request"), Budget.CanRequest(1, 4, 6));
    TestTrue(TEXT("first request consumes one unit"), Budget.TryConsume(1, 4, 6));
    TestEqual(TEXT("budget decrements exactly once"), Budget.TeamOneRemaining, 1);
    TestFalse(TEXT("team cooldown blocks an immediate repeat"), Budget.CanRequest(1, 5, 6));
    TestTrue(TEXT("other team has an independent cooldown"), Budget.CanRequest(2, 4, 6));

    Budget.Tick(24.9f);
    TestFalse(TEXT("partial cooldown remains gated"), Budget.CanRequest(1, 5, 6));
    Budget.Tick(0.1f);
    TestTrue(TEXT("elapsed cooldown restores authorization"), Budget.CanRequest(1, 5, 6));
    TestFalse(TEXT("population cap blocks reinforcement"), Budget.CanRequest(1, 6, 6));
    TestFalse(TEXT("unknown team cannot consume budget"), Budget.TryConsume(99, 0, 6));

    TestTrue(TEXT("final team two request succeeds"), Budget.TryConsume(2, 4, 6));
    Budget.Tick(25.0f);
    TestFalse(TEXT("exhausted budget stays unavailable"), Budget.TryConsume(2, 4, 6));
    TestEqual(TEXT("budget never becomes negative"), Budget.TeamTwoRemaining, 0);
    return true;
}

#endif
