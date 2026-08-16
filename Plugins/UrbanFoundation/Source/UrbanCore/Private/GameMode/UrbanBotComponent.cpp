#include "GameMode/UrbanBotComponent.h"

#include "AI/UrbanBotAIController.h"
#include "AI/UrbanSimpleBotComponent.h"
#include "AIController.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraBotCreationComponent.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameModes/LyraGameMode.h"
#include "Player/LyraPlayerBotController.h"
#include "Player/LyraPlayerSpawningManagerComponent.h"
#include "Player/LyraPlayerState.h"
#include "Teams/LyraTeamCreationComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UrbanBotComponent)

DEFINE_LOG_CATEGORY_STATIC(LogUrbanBot, Log, All);

namespace UrbanBot
{
	const TCHAR* TeamSetupPath = TEXT("/ShooterCore/Game/B_TeamSetup_TwoTeams.B_TeamSetup_TwoTeams_C");
	const TCHAR* SpawningRulesPath = TEXT("/ShooterCore/Game/B_TeamSpawningRules.B_TeamSpawningRules_C");
}

UUrbanBotComponent::UUrbanBotComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	BotControllerClass = AUrbanBotAIController::StaticClass();

	TeamSetupComponentClass = TSoftClassPtr<ULyraTeamCreationComponent>(FSoftObjectPath(UrbanBot::TeamSetupPath));
	SpawningRulesComponentClass = TSoftClassPtr<ULyraPlayerSpawningManagerComponent>(FSoftObjectPath(UrbanBot::SpawningRulesPath));
}

void UUrbanBotComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogUrbanBot, Error, TEXT("BeginPlay: no world."));
		return;
	}

	AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		UE_LOG(LogUrbanBot, Error, TEXT("BeginPlay: no game state yet."));
		return;
	}

	ULyraExperienceManagerComponent* ExperienceComponent =
		GameState->FindComponentByClass<ULyraExperienceManagerComponent>();
	if (!ExperienceComponent)
	{
		UE_LOG(LogUrbanBot, Error, TEXT("BeginPlay: no experience manager on game state."));
		return;
	}

	UE_LOG(LogUrbanBot, Log, TEXT("BeginPlay: registering experience callback on %s."),
		*GetNameSafe(GameState));
	ExperienceComponent->CallOrRegister_OnExperienceLoaded_LowPriority(
		FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void UUrbanBotComponent::TickComponent(
	const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ReinforcementBudget.Tick(DeltaTime);
}

void UUrbanBotComponent::OnExperienceLoaded(const ULyraExperienceDefinition* Experience)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogUrbanBot, Error, TEXT("OnExperienceLoaded: no world."));
		return;
	}

	AGameStateBase* GameState = World->GetGameState();

	// Disable the stock behavior-tree bot pipeline. The camera experience
	// mounts B_ShooterBotSpawner (a ULyraBotCreationComponent) through its
	// action sets, but this component is the only supported bot pipeline in
	// the arena. Running both at once makes two batches of bots race for the
	// same spawn points and overwrite each other's team assignments, and the
	// stock bots wait on team signals they never reliably get. The stock
	// spawner only fires once when the experience loads, so deactivating it
	// here (after it has already run) stops any further spawning; bots it did
	// spawn are destroyed below.
	if (GameState)
	{
			if (ULyraBotCreationComponent* StockSpawner =
					GameState->FindComponentByClass<ULyraBotCreationComponent>())
			{
				StockSpawner->Deactivate();
				StockSpawner->SetComponentTickEnabled(false);
			}

		// Stock bots use ALyraPlayerBotController (or subclasses such as
		// B_AI_Controller_LyraShooter); our tick bots use AUrbanBotAIController,
		// so only the stock batch is collected and destroyed here.
		TArray<ALyraPlayerBotController*> StockBots;
		for (TActorIterator<ALyraPlayerBotController> It(World); It; ++It)
		{
			StockBots.Add(*It);
		}
		for (ALyraPlayerBotController* StockBot : StockBots)
		{
			if (StockBot)
			{
				if (APawn* StockPawn = StockBot->GetPawn())
				{
					StockPawn->Destroy();
				}
				StockBot->Destroy();
			}
		}
	}

	EnsureTeamAndSpawningComponents(GameState);
	AssignTeamsToPlayers(GameState);
	SpawnBots();

	// Listen for future player logins (including the human player, who
	// usually spawns after the experience loads) so every new player gets a
	// team too - without a team the bots perceive them as neutral and never
	// attack.
	if (ALyraGameMode* LyraGameMode = Cast<ALyraGameMode>(World->GetAuthGameMode()))
	{
		LyraGameMode->OnGameModePlayerInitialized.AddUObject(this, &ThisClass::OnPlayerInitialized);
	}
}

void UUrbanBotComponent::OnPlayerInitialized(AGameModeBase* GameMode, AController* NewPlayer)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !NewPlayer || !NewPlayer->PlayerState)
	{
		return;
	}

	ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(NewPlayer->PlayerState);
	if (!LyraPS || (LyraPS->GetTeamId() != INDEX_NONE))
	{
		return;
	}

	UWorld* World = GetWorld();
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	// Pick the team with fewer members (bots already take both sides, so the
	// human joins whichever side has fewer players).
	int32 Team1Count = 0;
	int32 Team2Count = 0;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (const ALyraPlayerState* ExistingPS = Cast<ALyraPlayerState>(PS))
		{
			if (ExistingPS->GetTeamId() == 1)
			{
				++Team1Count;
			}
			else if (ExistingPS->GetTeamId() == 2)
			{
				++Team2Count;
			}
		}
	}

	const int32 NewTeam = (Team1Count <= Team2Count) ? 1 : 2;
	LyraPS->SetGenericTeamId(FGenericTeamId(NewTeam));
}

void UUrbanBotComponent::EnsureTeamAndSpawningComponents(AGameStateBase* GameState)
{
	if (!GameState)
	{
		UE_LOG(LogUrbanBot, Error, TEXT("EnsureTeamAndSpawningComponents: no game state."));
		return;
	}

	// Teams: the experience mounts B_TeamSetup_TwoTeams on the game state; if
	// it is missing (it should not be), fall back to our own component so the
	// team ids exist for the perception logic.
	if (!GameState->FindComponentByClass<ULyraTeamCreationComponent>())
	{
		if (UClass* ComponentClass = TeamSetupComponentClass.LoadSynchronous())
		{
			if (ULyraTeamCreationComponent* Component = NewObject<ULyraTeamCreationComponent>(GameState, ComponentClass))
			{
				Component->RegisterComponent();
			}
		}
	}

	// TDM spawn selection rules.
	if (!GameState->FindComponentByClass<ULyraPlayerSpawningManagerComponent>())
	{
		if (UClass* ComponentClass = SpawningRulesComponentClass.LoadSynchronous())
		{
			if (ULyraPlayerSpawningManagerComponent* Component = NewObject<ULyraPlayerSpawningManagerComponent>(GameState, ComponentClass))
			{
				Component->RegisterComponent();
			}
		}
	}
}

void UUrbanBotComponent::AssignTeamsToPlayers(AGameStateBase* GameState)
{
	if (!GameState)
	{
		return;
	}

	// Team ids 1 (Red) and 2 (Blue), matching B_TeamSetup_TwoTeams. Give every
	// existing player state a team, keeping the sides balanced.
	int32 Team1Count = 0;
	int32 Team2Count = 0;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (const ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(PS))
		{
			const int32 TeamId = LyraPS->GetTeamId();
			if (TeamId == 1)
			{
				++Team1Count;
			}
			else if (TeamId == 2)
			{
				++Team2Count;
			}
		}
	}

	for (APlayerState* PS : GameState->PlayerArray)
	{
		ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(PS);
		if (!LyraPS || (LyraPS->GetTeamId() != INDEX_NONE))
		{
			continue;
		}

		const int32 NewTeam = (Team1Count <= Team2Count) ? 1 : 2;
		LyraPS->SetGenericTeamId(FGenericTeamId(NewTeam));
		(NewTeam == 1) ? ++Team1Count : ++Team2Count;
	}
}

void UUrbanBotComponent::SpawnBots()
{
	static const EUrbanEnemyArchetype ArchetypeCycle[] = {
		EUrbanEnemyArchetype::Rifleman,
		EUrbanEnemyArchetype::Assault,
		EUrbanEnemyArchetype::Marksman,
		EUrbanEnemyArchetype::Drone
	};

	for (int32 Count = 0; Count < NumBotsToCreate; ++Count)
	{
		const int32 TeamId = Count % 2 == 0 ? 1 : 2;
		SpawnBotForTeam(TeamId, ArchetypeCycle[Count % UE_ARRAY_COUNT(ArchetypeCycle)], false);
	}
}

int32 UUrbanBotComponent::CountBotsForTeam(const int32 TeamId) const
{
	int32 Count = 0;
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AUrbanBotAIController> It(World); It; ++It)
		{
			const ALyraPlayerState* PlayerState = It->GetPlayerState<ALyraPlayerState>();
			Count += PlayerState && PlayerState->GetTeamId() == TeamId ? 1 : 0;
		}
	}
	return Count;
}

bool UUrbanBotComponent::CanRequestReinforcement(const int32 TeamId) const
{
	return ReinforcementBudget.CanRequest(TeamId, CountBotsForTeam(TeamId), MaxBotsPerTeam);
}

bool UUrbanBotComponent::TryRequestReinforcement(const int32 TeamId)
{
	const int32 Population = CountBotsForTeam(TeamId);
	if (!ReinforcementBudget.TryConsume(TeamId, Population, MaxBotsPerTeam))
	{
		return false;
	}

	const EUrbanEnemyArchetype Archetype = Population % 2 == 0
		? EUrbanEnemyArchetype::Rifleman
		: EUrbanEnemyArchetype::Assault;
	if (!SpawnBotForTeam(TeamId, Archetype, true))
	{
		// Refund a failed spawn without bypassing the cooldown safety gate.
		if (TeamId == 1)
		{
			++ReinforcementBudget.TeamOneRemaining;
		}
		else if (TeamId == 2)
		{
			++ReinforcementBudget.TeamTwoRemaining;
		}
		return false;
	}

	UE_LOG(LogUrbanBot, Log, TEXT("Team %d deployed reinforcement (%d/%d bots)."),
		TeamId, Population + 1, MaxBotsPerTeam);
	return true;
}

AAIController* UUrbanBotComponent::SpawnBotForTeam(
	const int32 TeamId, const EUrbanEnemyArchetype Archetype, const bool bIsReinforcement)
{
	UWorld* World = GetWorld();
	ALyraGameMode* LyraGameMode = World ? Cast<ALyraGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!World || !LyraGameMode || !BotControllerClass)
	{
		UE_LOG(LogUrbanBot, Error, TEXT("SpawnBotForTeam: missing world, game mode, or controller class."));
		return nullptr;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags |= RF_Transient;
	AAIController* NewController = World->SpawnActor<AAIController>(
		BotControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (!NewController)
	{
		return nullptr;
	}

	UUrbanSimpleBotComponent* SimpleAI = NewController->FindComponentByClass<UUrbanSimpleBotComponent>();
	if (!SimpleAI)
	{
		SimpleAI = NewObject<UUrbanSimpleBotComponent>(NewController);
		SimpleAI->RegisterComponent();
	}
	SimpleAI->SetEnemyArchetype(Archetype);

	const int32 BotSerial = NextBotSerial++;
	NewController->InitPlayerState();
	if (ALyraPlayerState* BotPlayerState = NewController->GetPlayerState<ALyraPlayerState>())
	{
		BotPlayerState->SetGenericTeamId(FGenericTeamId(static_cast<uint8>(TeamId)));
		BotPlayerState->SetPlayerName(bIsReinforcement
			? FString::Printf(TEXT("Reinforcement %d"), BotSerial)
			: FString::Printf(TEXT("Bot %d"), BotSerial));
	}
	else
	{
		NewController->Destroy();
		return nullptr;
	}

	LyraGameMode->GenericPlayerInitialization(NewController);
	LyraGameMode->RestartPlayer(NewController);
	if (APawn* BotPawn = NewController->GetPawn())
	{
		if (UTextRenderComponent* Label = NewObject<UTextRenderComponent>(BotPawn, TEXT("BotLabel")))
		{
			Label->SetupAttachment(BotPawn->GetRootComponent());
			Label->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
			Label->SetText(FText::FromString(bIsReinforcement
				? FString::Printf(TEXT("REINFORCEMENT %d"), BotSerial)
				: FString::Printf(TEXT("BOT %d"), BotSerial)));
			Label->SetTextRenderColor(bIsReinforcement ? FColor::Orange : FColor::Red);
			Label->SetWorldSize(80.0f);
			Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
			Label->RegisterComponent();
		}
		if (ULyraPawnExtensionComponent* PawnExt = BotPawn->FindComponentByClass<ULyraPawnExtensionComponent>())
		{
			PawnExt->CheckDefaultInitialization();
		}
		SimpleAI->EnsureVisibleMesh(BotPawn);
		SimpleAI->EquipStarterWeapon(BotPawn);
	}
	return NewController;
}
