#pragma once

#include "AIController.h"

#include "UrbanBotAIController.generated.h"

/**
 * Minimal bot controller used by the Urban arena bots.
 *
 * Deliberately plain: it requests a PlayerState (so the bot is tracked by
 * the game state and can be assigned a team) but runs NO behavior tree and
 * no perception component - all AI is driven by UUrbanSimpleBotComponent,
 * which is attached when the bot is spawned. This keeps the behavior fully
 * deterministic and easy to reason about.
 */
UCLASS()
class URBANCORE_API AUrbanBotAIController : public AAIController
{
	GENERATED_BODY()

public:
	AUrbanBotAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
