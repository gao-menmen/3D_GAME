#include "Character/UrbanPerspectiveTypes.h"

namespace
{
FUrbanPerspectiveDecision Reject(EUrbanPerspective Current, EUrbanPerspectiveBlockReason Reason)
{
    FUrbanPerspectiveDecision Decision;
    Decision.AcceptedPerspective = Current;
    Decision.Reason = Reason;
    return Decision;
}

FUrbanPerspectiveDecision Accept(EUrbanPerspective Requested)
{
    FUrbanPerspectiveDecision Decision;
    Decision.bAccepted = true;
    Decision.AcceptedPerspective = Requested;
    return Decision;
}

bool IsPlayerSelectable(EUrbanPerspective Perspective)
{
    return Perspective == EUrbanPerspective::FirstPerson
        || Perspective == EUrbanPerspective::ThirdPersonRight
        || Perspective == EUrbanPerspective::ThirdPersonLeft;
}
}

FUrbanPerspectiveDecision FUrbanPerspectiveRules::Evaluate(
    EUrbanPerspective Current,
    EUrbanPerspective Requested,
    EUrbanViewPolicy Policy,
    const FUrbanCharacterViewState& State)
{
    if (!IsPlayerSelectable(Requested))
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::InvalidRequest);
    }

    if (!State.bInitialized)
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::Uninitialized);
    }

    if (Policy == EUrbanViewPolicy::FirstPersonOnly)
    {
        return Reject(EUrbanPerspective::ForcedFirstPerson, EUrbanPerspectiveBlockReason::Policy);
    }

    if (Policy == EUrbanViewPolicy::ThirdPersonOnly)
    {
        if (Requested == EUrbanPerspective::FirstPerson)
        {
            const EUrbanPerspective Fallback = Current == EUrbanPerspective::ThirdPersonLeft
                ? EUrbanPerspective::ThirdPersonLeft
                : EUrbanPerspective::ThirdPersonRight;
            return Reject(Fallback, EUrbanPerspectiveBlockReason::Policy);
        }
    }
    else if (Policy == EUrbanViewPolicy::Disabled)
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::Policy);
    }

    if (State.bDead)
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::Dead);
    }
    if (State.bDowned)
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::Downed);
    }
    if (State.bTraversal)
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::Traversal);
    }
    if (State.bSprinting)
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::Sprinting);
    }
    if (State.bAiming)
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::Aiming);
    }
    if (State.bTransitioning)
    {
        return Reject(Current, EUrbanPerspectiveBlockReason::Transitioning);
    }

    return Accept(Requested);
}
