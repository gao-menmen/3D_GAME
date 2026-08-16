#include "AI/UrbanAITypes.h"
void FUrbanReinforcementBudget::Tick(const float DeltaTime)
{
    const float SafeDelta = FMath::Max(DeltaTime, 0.0f);
    TeamOneCooldownRemainingSeconds = FMath::Max(TeamOneCooldownRemainingSeconds - SafeDelta, 0.0f);
    TeamTwoCooldownRemainingSeconds = FMath::Max(TeamTwoCooldownRemainingSeconds - SafeDelta, 0.0f);
}

bool FUrbanReinforcementBudget::CanRequest(
    const int32 TeamId, const int32 CurrentTeamPopulation, const int32 MaxTeamPopulation) const
{
    if (CurrentTeamPopulation >= FMath::Max(MaxTeamPopulation, 0))
    {
        return false;
    }

    if (TeamId == 1)
    {
        return TeamOneRemaining > 0 && TeamOneCooldownRemainingSeconds <= KINDA_SMALL_NUMBER;
    }
    if (TeamId == 2)
    {
        return TeamTwoRemaining > 0 && TeamTwoCooldownRemainingSeconds <= KINDA_SMALL_NUMBER;
    }
    return false;
}

bool FUrbanReinforcementBudget::TryConsume(
    const int32 TeamId, const int32 CurrentTeamPopulation, const int32 MaxTeamPopulation)
{
    if (!CanRequest(TeamId, CurrentTeamPopulation, MaxTeamPopulation))
    {
        return false;
    }

    const float SafeCooldown = FMath::Max(RequestCooldownSeconds, 0.0f);
    if (TeamId == 1)
    {
        --TeamOneRemaining;
        TeamOneCooldownRemainingSeconds = SafeCooldown;
    }
    else
    {
        --TeamTwoRemaining;
        TeamTwoCooldownRemainingSeconds = SafeCooldown;
    }
    return true;
}


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

bool UUrbanAIDecisionLibrary::HasCompletedReaction(const float ConfirmedTargetSeconds, const float ReactionTimeSeconds)
{
    return FMath::Max(ConfirmedTargetSeconds, 0.0f) >= FMath::Max(ReactionTimeSeconds, 0.0f);
}
FVector UUrbanAIDecisionLibrary::BuildFlankCandidate(
    const FVector& PawnLocation,
    const FVector& EnemyLocation,
    const float FlankPreference,
    const int32 SideSign)
{
    const FVector ToEnemy = (EnemyLocation - PawnLocation).GetSafeNormal2D();
    if (ToEnemy.IsNearlyZero())
    {
        return EnemyLocation;
    }

    const FVector Side = FVector::CrossProduct(FVector::UpVector, ToEnemy).GetSafeNormal2D();
    const float SafePreference = FMath::Clamp(FlankPreference, 0.0f, 1.0f);
    const float SafeSideSign = SideSign >= 0 ? 1.0f : -1.0f;
    const float LateralDistance = FMath::Lerp(500.0f, 1100.0f, SafePreference);
    return EnemyLocation - ToEnemy * 650.0f + Side * SafeSideSign * LateralDistance;
}