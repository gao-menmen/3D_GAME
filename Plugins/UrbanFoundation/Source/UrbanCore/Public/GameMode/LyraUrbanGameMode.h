#pragma once

#include "GameModes/LyraGameMode.h"

#include "LyraUrbanGameMode.generated.h"

class UUrbanBotComponent;

/**
 * Game mode for the Urban arena maps. Adds the bot/team integration
 * component (UUrbanBotComponent) which mounts the ShooterCore bot pipeline
 * onto the game state once the experience has loaded.
 */
UCLASS()
class URBANCORE_API ALyraUrbanGameMode : public ALyraGameMode
{
	GENERATED_BODY()

public:
	ALyraUrbanGameMode(const FObjectInitializer& ObjectInitializer);

private:
	UPROPERTY()
	TObjectPtr<UUrbanBotComponent> UrbanBotComponent;
};
