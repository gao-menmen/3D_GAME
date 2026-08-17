#pragma once

#include "AI/UrbanAITypes.h"
#include "Components/ActorComponent.h"

#include "UrbanBotComponent.generated.h"

class AAIController;
class ULyraExperienceDefinition;
class ULyraPlayerSpawningManagerComponent;
class ULyraTeamCreationComponent;

/**
 * Integrates the ShooterCore bot pipeline into the arena map.
 *
 * The camera experience lists the bot assets in its bundles but does not
 * mount the stock bot components the way the elimination experience does, so
 * this component (living on the game mode via ALyraUrbanGameMode) takes over
 * the stock steps directly once the experience has loaded:
 *   1. ensure the team-creation and spawn-rule components exist on the game
 *      state,
 *   2. assign every player (including the human) to a team, and keep
 *      assigning teams as new players spawn,
 *   3. spawn the ShooterCore AI bot controllers (B_AI_Controller_LyraShooter,
 *      which carries AIPerception + the shooting behavior tree) and give each
 *      a team, so about half the bots are enemies.
 * The bot controller class is hard-referenced through ConstructorHelpers so
 * it is always cooked into packaged builds.
 */
UCLASS()
class URBANCORE_API UUrbanBotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUrbanBotComponent(const FObjectInitializer& ObjectInitializer);

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UActorComponent interface

	bool CanRequestReinforcement(int32 TeamId) const;
	bool TryRequestReinforcement(int32 TeamId);

private:
	void OnExperienceLoaded(const ULyraExperienceDefinition* Experience);
	void OnPlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer);
	void EnsureTeamAndSpawningComponents(AGameStateBase* GameState);
	void AssignTeamsToPlayers(AGameStateBase* GameState);
	void SpawnBots();
	AAIController* SpawnBotForTeam(int32 TeamId, EUrbanEnemyArchetype Archetype, bool bIsReinforcement);
	int32 CountBotsForTeam(int32 TeamId) const;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	int32 NumBotsToCreate = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Reinforcement", meta = (ClampMin = "1"))
	int32 MaxBotsPerTeam = 6;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Reinforcement")
	FUrbanReinforcementBudget ReinforcementBudget;

	int32 NextBotSerial = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	TSubclassOf<AAIController> BotControllerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	TSoftClassPtr<ULyraTeamCreationComponent> TeamSetupComponentClass;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	TSoftClassPtr<ULyraPlayerSpawningManagerComponent> SpawningRulesComponentClass;
};
