#include "AI/UrbanAITypes.h"

void FUrbanEnemyArchetypeTuning::Sanitize()
{
    PreferredEngagementRangeMeters = FMath::Clamp(PreferredEngagementRangeMeters, 2.0f, 200.0f);
    ReactionTimeSeconds = FMath::Clamp(ReactionTimeSeconds, 0.1f, 3.0f);
    AimSpreadDegrees = FMath::Clamp(AimSpreadDegrees, 0.1f, 15.0f);
    CoverPreference = FMath::Clamp(CoverPreference, 0.0f, 1.0f);
    FlankPreference = FMath::Clamp(FlankPreference, 0.0f, 1.0f);
}

FUrbanEnemyArchetypeTuning FUrbanEnemyArchetypeTuning::MakeDefaults(const EUrbanEnemyArchetype InArchetype)
{
    FUrbanEnemyArchetypeTuning Tuning;
    Tuning.Archetype = InArchetype;

    switch (InArchetype)
    {
    case EUrbanEnemyArchetype::Rifleman:
        Tuning.PreferredEngagementRangeMeters = 30.0f;
        Tuning.ReactionTimeSeconds = 0.65f;
        Tuning.AimSpreadDegrees = 2.0f;
        Tuning.CoverPreference = 0.9f;
        Tuning.FlankPreference = 0.3f;
        break;
    case EUrbanEnemyArchetype::Assault:
        Tuning.PreferredEngagementRangeMeters = 12.0f;
        Tuning.ReactionTimeSeconds = 0.45f;
        Tuning.AimSpreadDegrees = 2.8f;
        Tuning.CoverPreference = 0.55f;
        Tuning.FlankPreference = 0.85f;
        Tuning.bCanUseGrenades = true;
        break;
    case EUrbanEnemyArchetype::Marksman:
        Tuning.PreferredEngagementRangeMeters = 75.0f;
        Tuning.ReactionTimeSeconds = 0.9f;
        Tuning.AimSpreadDegrees = 0.7f;
        Tuning.CoverPreference = 0.95f;
        Tuning.FlankPreference = 0.1f;
        break;
    case EUrbanEnemyArchetype::Drone:
        Tuning.PreferredEngagementRangeMeters = 40.0f;
        Tuning.ReactionTimeSeconds = 0.35f;
        Tuning.AimSpreadDegrees = 1.8f;
        Tuning.CoverPreference = 0.1f;
        Tuning.FlankPreference = 0.65f;
        Tuning.bIsAerial = true;
        break;
    default:
        break;
    }

    Tuning.Sanitize();
    return Tuning;
}

EUrbanAIBehaviorState UUrbanAIDecisionLibrary::ResolveBehaviorState(const FUrbanAIDecisionContext& Context)
{
    const float SafeTimeInState = FMath::Max(Context.TimeInStateSeconds, 0.0f);

    if (Context.bHasConfirmedTarget)
    {
        if (Context.bReinforcementBudgetAvailable && SafeTimeInState >= 12.0f)
        {
            return EUrbanAIBehaviorState::CallReinforcement;
        }
        if (Context.bHasValidFlankRoute && SafeTimeInState >= 5.0f)
        {
            return EUrbanAIBehaviorState::Flank;
        }
        return EUrbanAIBehaviorState::CoverCombat;
    }

    if (Context.bHasRecentStimulus)
    {
        return Context.bReachedLastKnownLocation
            ? EUrbanAIBehaviorState::Search
            : EUrbanAIBehaviorState::Investigate;
    }

    return EUrbanAIBehaviorState::PatrolOrGuard;
}
