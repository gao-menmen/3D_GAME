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
	UWorld* World = GetWorld();
	ALyraGameMode* LyraGameMode = World ? Cast<ALyraGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!World || !LyraGameMode)
	{
		UE_LOG(LogUrbanBot, Error, TEXT("SpawnBots: no world/lyra game mode."));
		return;
	}

	if (!BotControllerClass)
	{
		UE_LOG(LogUrbanBot, Error, TEXT("SpawnBots: bot controller class not set."));
		return;
	}

	for (int32 Count = 0; Count < NumBotsToCreate; ++Count)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.ObjectFlags |= RF_Transient;

		AAIController* NewController = World->SpawnActor<AAIController>(
			BotControllerClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnInfo);
		if (!NewController)
		{
			UE_LOG(LogUrbanBot, Error, TEXT("SpawnBots: failed to spawn controller %d."), Count);
			continue;
		}

		// Attach the simple tick-driven AI: movement, turning, shooting,
		// 120-degree vision, weapon-based decision, flee and patrol.
		UUrbanSimpleBotComponent* SimpleAI = NewController->FindComponentByClass<UUrbanSimpleBotComponent>();
		if (SimpleAI == nullptr)
		{
			SimpleAI = NewObject<UUrbanSimpleBotComponent>(NewController);
			SimpleAI->RegisterComponent();
		}

		// Give each bot a weapon: pistols, rifles and shotguns mix across
		// both teams, with different damage/fire-rate/range per weapon.
		static const EBotWeaponType WeaponCycle[] = {
			EBotWeaponType::Pistol,
			EBotWeaponType::Rifle,
			EBotWeaponType::Shotgun,
			EBotWeaponType::Rifle
		};
		SimpleAI->SetWeaponType(WeaponCycle[Count % (sizeof(WeaponCycle) / sizeof(WeaponCycle[0]))]);

		// Create the player state and assign the team BEFORE RestartPlayer.
		// The TDM spawn-point selector resolves a controller's team through
		// its player state, and the state only exists once the pawn is
		// possessed - so without this the bot would be treated as teamless
		// during spawn selection (triggering the ensure and falling back to
		// a random start) and the bot name below would be a no-op.
			NewController->InitPlayerState();
			if (ALyraPlayerState* BotPlayerState = NewController->GetPlayerState<ALyraPlayerState>())
			{
				const FGenericTeamId BotTeamId = (Count % 2 == 0) ? FGenericTeamId(1) : FGenericTeamId(2);
				BotPlayerState->SetGenericTeamId(BotTeamId);
				BotPlayerState->SetPlayerName(FString::Printf(TEXT("Bot %d"), Count));
			}
			else
			{
				UE_LOG(LogUrbanBot, Error, TEXT("SpawnBots: bot %d failed to create a player state."), Count);
			}

		LyraGameMode->GenericPlayerInitialization(NewController);
		LyraGameMode->RestartPlayer(NewController);

		if (APawn* BotPawn = NewController->GetPawn())
		{
			// Red "BOT n" label above the head so the player can tell bots
			// apart from their own pawn at a glance.
			if (UTextRenderComponent* Label = NewObject<UTextRenderComponent>(BotPawn, TEXT("BotLabel")))
			{
				Label->SetupAttachment(BotPawn->GetRootComponent());
				Label->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
				Label->SetText(FText::FromString(FString::Printf(TEXT("BOT %d"), Count)));
				Label->SetTextRenderColor(FColor::Red);
				Label->SetWorldSize(80.0f);
				Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
				Label->RegisterComponent();
			}

			if (ULyraPawnExtensionComponent* PawnExtComponent =
					BotPawn->FindComponentByClass<ULyraPawnExtensionComponent>())
			{
				PawnExtComponent->CheckDefaultInitialization();
			}

			// The hero body is an invisible placeholder mesh (first-person
			// camera milestone); give the bot a visible Mannequin body so the
			// player can actually see it.
			SimpleAI->EnsureVisibleMesh(BotPawn);
			SimpleAI->EquipStarterWeapon(BotPawn);
		}
	}
}
