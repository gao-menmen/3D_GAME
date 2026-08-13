// Copyright Epic Games, Inc. All Rights Reserved.

#include "TDM_PlayerSpawningManagmentComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraGameState.h"
#include "Player/LyraPlayerStart.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TDM_PlayerSpawningManagmentComponent)

class AActor;

UTDM_PlayerSpawningManagmentComponent::UTDM_PlayerSpawningManagmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AActor* UTDM_PlayerSpawningManagmentComponent::OnChoosePlayerStart(AController* Player, TArray<ALyraPlayerStart*>& PlayerStarts)
{
	ULyraTeamSubsystem* TeamSubsystem = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	if (!ensure(TeamSubsystem))
	{
		return nullptr;
	}

	const int32 PlayerTeamId = TeamSubsystem->FindTeamFromObject(Player);

	// We should have a TeamId by now, but early login stuff before post login can try to do stuff, ignore it.
	if (!ensure(PlayerTeamId != INDEX_NONE))
	{
		return nullptr;
	}

	ALyraGameState* GameState = GetGameStateChecked<ALyraGameState>();

	// Split the arena's spawn points by team zone so the two teams start and
	// respawn on opposite halves of the map instead of interleaving: team 1
	// owns the right half (X >= 0), team 2 the left half (X < 0). The test
	// arena's ten LyraPlayerStarts are placed symmetrically, five per half.
	TArray<ALyraPlayerStart*> TeamZoneStarts;
	TeamZoneStarts.Reserve(PlayerStarts.Num());
	for (ALyraPlayerStart* Start : PlayerStarts)
	{
		const bool bRightHalf = Start->GetActorLocation().X >= 0.0f;
		const bool bTeamOwnsRight = (PlayerTeamId == 1);
		if (bRightHalf == bTeamOwnsRight)
		{
			TeamZoneStarts.Add(Start);
		}
	}
	if (TeamZoneStarts.Num() == 0)
	{
		TeamZoneStarts = PlayerStarts; // fallback if no zone starts exist
	}

	ALyraPlayerStart* BestPlayerStart = nullptr;
	double MaxDistance = 0;
	ALyraPlayerStart* FallbackPlayerStart = nullptr;
	double FallbackMaxDistance = 0;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		const int32 TeamId = TeamSubsystem->FindTeamFromObject(PS);
		
		// We should have a TeamId by now, but other players can legitimately
		// still be mid-spawn (the bot/team pipeline assigns teams slightly
		// after player state creation), so a missing team here is a skip
		// condition rather than an error.
		if (PS->IsOnlyASpectator() || TeamId == INDEX_NONE)
		{
			continue;
		}

		// If the other player isn't on the same team, lets find the furthest spawn from them.
		if (TeamId != PlayerTeamId)
		{
			for (ALyraPlayerStart* PlayerStart : TeamZoneStarts)
			{
				if (APawn* Pawn = PS->GetPawn())
				{
					const double Distance = PlayerStart->GetDistanceTo(Pawn);

					if (PlayerStart->IsClaimed())
					{
						if (FallbackPlayerStart == nullptr || Distance > FallbackMaxDistance)
						{
							FallbackPlayerStart = PlayerStart;
							FallbackMaxDistance = Distance;
						}
					}
					else if (PlayerStart->GetLocationOccupancy(Player) < ELyraPlayerStartLocationOccupancy::Full)
					{
						if (BestPlayerStart == nullptr || Distance > MaxDistance)
						{
							BestPlayerStart = PlayerStart;
							MaxDistance = Distance;
						}
					}
				}
			}
		}
	}

	if (BestPlayerStart)
	{
		return BestPlayerStart;
	}

	return FallbackPlayerStart;
}

void UTDM_PlayerSpawningManagmentComponent::OnFinishRestartPlayer(AController* Player, const FRotator& StartRotation)
{
	
}
