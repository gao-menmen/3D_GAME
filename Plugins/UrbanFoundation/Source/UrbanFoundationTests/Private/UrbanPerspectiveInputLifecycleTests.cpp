#include "Character/UrbanPerspectiveComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/UrbanPerspectiveInputComponent.h"
#include "Character/LyraHeroComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectiveInputLifecycleTest,
    "UrbanSpear.CharacterCamera.Input.BindsWhenLyraInputBecomesReady",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectiveInputLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    TestNotNull(TEXT("standalone game instance exists"), GameInstance);
    if (!GameInstance)
    {
        return false;
    }

    GameInstance->InitializeStandalone();
    UWorld* World = GameInstance->GetWorld();
    TestNotNull(TEXT("standalone game world exists"), World);
    if (!World)
    {
        GameInstance->Shutdown();
        return false;
    }

    APawn* Pawn = World->SpawnActor<APawn>();
    TestNotNull(TEXT("test pawn exists"), Pawn);
    if (!Pawn)
    {
        GameInstance->Shutdown();
        return false;
    }

    UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(Pawn);

    UUrbanPerspectiveComponent* PerspectiveComponent =
        NewObject<UUrbanPerspectiveComponent>(Pawn, TEXT("PerspectiveComponent"));
    UUrbanPerspectiveInputComponent* InputLifecycleComponent =
        NewObject<UUrbanPerspectiveInputComponent>(Pawn, TEXT("PerspectiveInputComponent"));
    Pawn->AddInstanceComponent(PerspectiveComponent);
    Pawn->AddInstanceComponent(InputLifecycleComponent);
    PerspectiveComponent->RegisterComponent();
    InputLifecycleComponent->RegisterComponent();

    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    Pawn->DispatchBeginPlay();

    TestTrue(
        TEXT("input lifecycle listens for Lyra extension events"),
        InputLifecycleComponent->InputExtensionRequestHandle.IsValid());

    TestNull(
        TEXT("input is initially unbound while the local input stack is unavailable"),
        InputLifecycleComponent->BoundInputComponent.Get());

    ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    GameInstance->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(0));

    APlayerController* PlayerController = World->SpawnActor<APlayerController>();
    PlayerController->SetPlayer(LocalPlayer);
    PlayerController->Possess(Pawn);

    UEnhancedInputComponent* EnhancedInputComponent =
        NewObject<UEnhancedInputComponent>(Pawn, TEXT("DelayedEnhancedInputComponent"));
    EnhancedInputComponent->RegisterComponent();
    Pawn->InputComponent = EnhancedInputComponent;

    UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
        Pawn,
        ULyraHeroComponent::NAME_BindInputsNow);

    TestEqual(
        TEXT("Lyra BindInputsNow retries Urban input binding after delayed setup"),
        InputLifecycleComponent->BoundInputComponent.Get(),
        EnhancedInputComponent);
    TestTrue(
        TEXT("the delayed binding installs both perspective actions"),
        InputLifecycleComponent->BindingHandles.Num() == 2);
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    UInputMappingContext* UrbanMappingContext = InputLifecycleComponent->BoundMappingContext.Get();
    TestNotNull(TEXT("the local player enhanced input subsystem exists"), InputSubsystem);
    TestNotNull(TEXT("the Urban mapping context is tracked after initial binding"), UrbanMappingContext);
    if (InputSubsystem && UrbanMappingContext)
    {
        TestTrue(
            TEXT("the initial Lyra input initialization installs the Urban mapping context"),
            InputSubsystem->HasMappingContext(UrbanMappingContext));

        const int32 InitialActionBindingCount = EnhancedInputComponent->GetActionEventBindings().Num();
        const int32 InitialBindingHandleCount = InputLifecycleComponent->BindingHandles.Num();

        InputSubsystem->ClearAllMappings();
        TestFalse(
            TEXT("Lyra clearing mappings removes the Urban mapping context"),
            InputSubsystem->HasMappingContext(UrbanMappingContext));

        UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
            Pawn,
            ULyraHeroComponent::NAME_BindInputsNow);

        TestTrue(
            TEXT("a repeated Lyra BindInputsNow restores the Urban mapping context"),
            InputSubsystem->HasMappingContext(UrbanMappingContext));
        TestEqual(
            TEXT("a repeated Lyra BindInputsNow does not duplicate action delegates"),
            EnhancedInputComponent->GetActionEventBindings().Num(),
            InitialActionBindingCount);
        TestEqual(
            TEXT("a repeated Lyra BindInputsNow preserves the tracked binding handles"),
            InputLifecycleComponent->BindingHandles.Num(),
            InitialBindingHandleCount);
    }

    UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(Pawn);
    GameInstance->Shutdown();
    return true;
}

#endif
