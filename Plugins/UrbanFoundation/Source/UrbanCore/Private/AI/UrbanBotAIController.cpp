#include "AI/UrbanBotAIController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UrbanBotAIController)

AUrbanBotAIController::AUrbanBotAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Give the bot a PlayerState so it is part of the game state's player
	// array and can be assigned a team (needed for enemy detection).
	bWantsPlayerState = true;
	bStopAILogicOnUnposses = false;
}
