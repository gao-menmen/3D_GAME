#include "Input/UrbanPerspectiveInputComponent.h"

#include "Character/UrbanPerspectiveComponent.h"
#include "Character/LyraHeroComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

UUrbanPerspectiveInputComponent::UUrbanPerspectiveInputComponent(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = false;

    TogglePerspectiveAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
        TEXT("/UrbanFoundation/Input/Actions/IA_UrbanTogglePerspective.IA_UrbanTogglePerspective")));
    ToggleShoulderAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
        TEXT("/UrbanFoundation/Input/Actions/IA_UrbanToggleShoulder.IA_UrbanToggleShoulder")));
    PerspectiveMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(
        TEXT("/UrbanFoundation/Input/Mappings/IMC_UrbanPerspective_KBM.IMC_UrbanPerspective_KBM")));
}

void UUrbanPerspectiveInputComponent::BeginPlay()
{
    Super::BeginPlay();

    if (APawn* Pawn = GetPawn<APawn>())
    {
        Pawn->ReceiveRestartedDelegate.AddDynamic(this, &ThisClass::HandlePawnRestarted);
        Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &ThisClass::HandleControllerChanged);

        if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        {
            if (UGameFrameworkComponentManager* ComponentManager =
                UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance))
            {
                UGameFrameworkComponentManager::FExtensionHandlerDelegate ExtensionDelegate =
                    UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
                        this,
                        &ThisClass::HandlePawnExtension);
                InputExtensionRequestHandle = ComponentManager->AddExtensionHandler(
                    APawn::StaticClass(),
                    ExtensionDelegate);
            }
        }
    }

    TryBindInput();
}

void UUrbanPerspectiveInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (APawn* Pawn = GetPawn<APawn>())
    {
        Pawn->ReceiveRestartedDelegate.RemoveDynamic(this, &ThisClass::HandlePawnRestarted);
        Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::HandleControllerChanged);
    }

    InputExtensionRequestHandle.Reset();
    RemoveInputBindings();
    PerspectiveComponent = nullptr;

    Super::EndPlay(EndPlayReason);
}

void UUrbanPerspectiveInputComponent::TryBindInput()
{
    APawn* Pawn = GetPawn<APawn>();
    APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
    UEnhancedInputComponent* EnhancedInputComponent = Pawn
        ? Cast<UEnhancedInputComponent>(Pawn->InputComponent)
        : nullptr;
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
        ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
        : nullptr;

    if (!PlayerController || !PlayerController->IsLocalController()
        || !EnhancedInputComponent || !InputSubsystem)
    {
        return;
    }

    if (BoundInputComponent == EnhancedInputComponent && BoundInputSubsystem == InputSubsystem)
    {
        return;
    }

    UInputAction* PerspectiveAction = TogglePerspectiveAction.LoadSynchronous();
    UInputAction* ShoulderAction = ToggleShoulderAction.LoadSynchronous();
    UInputMappingContext* MappingContext = PerspectiveMappingContext.LoadSynchronous();
    UUrbanPerspectiveComponent* ViewComponent = Pawn->FindComponentByClass<UUrbanPerspectiveComponent>();
    if (!PerspectiveAction || !ShoulderAction || !MappingContext || !ViewComponent)
    {
        return;
    }

    RemoveInputBindings();

    FModifyContextOptions Options;
    Options.bIgnoreAllPressedKeysUntilRelease = false;
    InputSubsystem->AddMappingContext(MappingContext, MappingPriority, Options);

    BindingHandles.Add(EnhancedInputComponent->BindAction(
        PerspectiveAction,
        ETriggerEvent::Triggered,
        this,
        &ThisClass::HandleTogglePerspective).GetHandle());
    BindingHandles.Add(EnhancedInputComponent->BindAction(
        ShoulderAction,
        ETriggerEvent::Triggered,
        this,
        &ThisClass::HandleToggleShoulder).GetHandle());

    BoundInputComponent = EnhancedInputComponent;
    BoundInputSubsystem = InputSubsystem;
    BoundMappingContext = MappingContext;
    PerspectiveComponent = ViewComponent;
}

void UUrbanPerspectiveInputComponent::RemoveInputBindings()
{
    if (BoundInputComponent)
    {
        for (const uint32 Handle : BindingHandles)
        {
            BoundInputComponent->RemoveBindingByHandle(Handle);
        }
    }
    BindingHandles.Reset();

    if (BoundInputSubsystem && BoundMappingContext)
    {
        BoundInputSubsystem->RemoveMappingContext(BoundMappingContext);
    }

    BoundInputComponent = nullptr;
    BoundInputSubsystem = nullptr;
    BoundMappingContext = nullptr;
}

void UUrbanPerspectiveInputComponent::HandleTogglePerspective()
{
    if (PerspectiveComponent)
    {
        PerspectiveComponent->RequestTogglePerspective();
    }
}

void UUrbanPerspectiveInputComponent::HandleToggleShoulder()
{
    if (PerspectiveComponent)
    {
        PerspectiveComponent->RequestToggleShoulder();
    }
}

void UUrbanPerspectiveInputComponent::HandlePawnExtension(AActor* Actor, FName EventName)
{
    if (Actor != GetPawn<APawn>())
    {
        return;
    }

    if (EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved
        || EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved)
    {
        RemoveInputBindings();
        PerspectiveComponent = nullptr;
    }
    else if (EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded
        || EventName == ULyraHeroComponent::NAME_BindInputsNow)
    {
        TryBindInput();
    }
}

void UUrbanPerspectiveInputComponent::HandlePawnRestarted(APawn* Pawn)
{
    if (Pawn == GetPawn<APawn>())
    {
        TryBindInput();
    }
}

void UUrbanPerspectiveInputComponent::HandleControllerChanged(
    APawn* Pawn,
    AController* OldController,
    AController* NewController)
{
    (void)OldController;
    (void)NewController;

    if (Pawn == GetPawn<APawn>())
    {
        RemoveInputBindings();
        PerspectiveComponent = nullptr;
        TryBindInput();
    }
}
