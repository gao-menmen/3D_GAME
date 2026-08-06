#include "CoreMinimal.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/UrbanCharacterStateComponent.h"
#include "Character/UrbanPerspectiveComponent.h"
#include "Character/UrbanPerspectivePresentationComponent.h"
#include "Character/UrbanViewPolicyComponent.h"
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
#include "Engine/Engine.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PrimitiveComponentUtilities.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UrbanPerspectiveComponentTests
{
    APawn* CreateTransientPawn()
    {
        return NewObject<APawn>(GetTransientPackage(), NAME_None, RF_Transient);
    }

    void AttachComponent(APawn* Pawn, UActorComponent* Component)
    {
        Pawn->AddInstanceComponent(Component);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanCharacterStateComponentTest,
    "UrbanSpear.CharacterCamera.Components.CharacterState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanCharacterStateComponentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveComponentTests;

    APawn* Pawn = CreateTransientPawn();
    ULyraAbilitySystemComponent* AbilitySystem = NewObject<ULyraAbilitySystemComponent>(Pawn);
    UUrbanCharacterStateComponent* StateComponent = NewObject<UUrbanCharacterStateComponent>(Pawn);
    AttachComponent(Pawn, AbilitySystem);
    AttachComponent(Pawn, StateComponent);

    const FUrbanCharacterViewState DefaultState = StateComponent->GetViewState();
    TestTrue(TEXT("default state is initialized"), DefaultState.bInitialized);
    TestFalse(TEXT("default state is not aiming"), DefaultState.bAiming);
    TestFalse(TEXT("default state is not sprinting"), DefaultState.bSprinting);
    TestFalse(TEXT("default state is not traversing"), DefaultState.bTraversal);
    TestFalse(TEXT("default state is not downed"), DefaultState.bDowned);
    TestFalse(TEXT("default state is not dead"), DefaultState.bDead);

    struct FTagExpectation
    {
        const TCHAR* TagName;
        bool FUrbanCharacterViewState::* Field;
        const TCHAR* Message;
    };

    const FTagExpectation Expectations[] = {
        {TEXT("Status.Aiming"), &FUrbanCharacterViewState::bAiming, TEXT("aiming tag maps to aiming state")},
        {TEXT("Status.Sprinting"), &FUrbanCharacterViewState::bSprinting, TEXT("sprinting tag maps to sprinting state")},
        {TEXT("Status.Traversal"), &FUrbanCharacterViewState::bTraversal, TEXT("traversal tag maps to traversal state")},
        {TEXT("Status.Downed"), &FUrbanCharacterViewState::bDowned, TEXT("downed tag maps to downed state")},
        {TEXT("Status.Death"), &FUrbanCharacterViewState::bDead, TEXT("death tag maps to dead state")},
    };

    for (const FTagExpectation& Expectation : Expectations)
    {
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Expectation.TagName));
        AbilitySystem->AddLooseGameplayTag(Tag);
        const FUrbanCharacterViewState TaggedState = StateComponent->GetViewState();
        TestTrue(Expectation.Message, TaggedState.*(Expectation.Field));
        AbilitySystem->RemoveLooseGameplayTag(Tag);
        const FUrbanCharacterViewState ClearedState = StateComponent->GetViewState();
        TestFalse(TEXT("removing a restriction tag clears its state"), ClearedState.*(Expectation.Field));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanViewPolicyComponentTest,
    "UrbanSpear.CharacterCamera.Components.ViewPolicyAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanViewPolicyComponentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveComponentTests;

    APawn* Pawn = CreateTransientPawn();
    Pawn->SetRole(ROLE_SimulatedProxy);
    UUrbanViewPolicyComponent* PolicyComponent = NewObject<UUrbanViewPolicyComponent>(Pawn);
    AttachComponent(Pawn, PolicyComponent);

    TestEqual(TEXT("default policy allows free choice"), PolicyComponent->GetPolicy(), EUrbanViewPolicy::FreeChoice);
    TestFalse(TEXT("non-authority cannot mutate policy"), PolicyComponent->SetPolicy(EUrbanViewPolicy::FirstPersonOnly));
    TestEqual(TEXT("rejected mutation preserves policy"), PolicyComponent->GetPolicy(), EUrbanViewPolicy::FreeChoice);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectiveStateMachineTest,
    "UrbanSpear.CharacterCamera.StateMachine.PerspectiveLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectiveStateMachineTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveComponentTests;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    APawn* Pawn = World->SpawnActor<APawn>();
    ULyraAbilitySystemComponent* AbilitySystem = NewObject<ULyraAbilitySystemComponent>(Pawn);
    UUrbanCharacterStateComponent* StateComponent = NewObject<UUrbanCharacterStateComponent>(Pawn);
    UUrbanViewPolicyComponent* PolicyComponent = NewObject<UUrbanViewPolicyComponent>(Pawn);
    UUrbanPerspectiveComponent* PerspectiveComponent = NewObject<UUrbanPerspectiveComponent>(Pawn);

    UActorComponent* Components[] = {
        AbilitySystem,
        StateComponent,
        PolicyComponent,
        PerspectiveComponent,
    };
    for (UActorComponent* Component : Components)
    {
        AttachComponent(Pawn, Component);
        Component->RegisterComponent();
    }

    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    Pawn->DispatchBeginPlay();
    TestTrue(TEXT("perspective component begins play with its owner"), PerspectiveComponent->HasBegunPlay());

    TestEqual(TEXT("accepted perspective defaults to first person"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::FirstPerson);
    TestEqual(TEXT("preferred perspective defaults to first person"), PerspectiveComponent->GetPreferredPerspective(), EUrbanPerspective::FirstPerson);

    PerspectiveComponent->ServerRequestPerspective(EUrbanPerspective::ThirdPersonRight);
    TestEqual(TEXT("legal request changes accepted perspective"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonRight);
    TestEqual(TEXT("legal request saves preferred perspective"), PerspectiveComponent->GetPreferredPerspective(), EUrbanPerspective::ThirdPersonRight);

    TestTrue(TEXT("authority can force first person"), PolicyComponent->SetPolicy(EUrbanViewPolicy::FirstPersonOnly));
    TestEqual(TEXT("forced policy changes accepted perspective"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ForcedFirstPerson);
    TestEqual(TEXT("forced policy preserves player preference"), PerspectiveComponent->GetPreferredPerspective(), EUrbanPerspective::ThirdPersonRight);

    TestTrue(TEXT("authority can restore free choice"), PolicyComponent->SetPolicy(EUrbanViewPolicy::FreeChoice));
    TestEqual(TEXT("ending force restores saved preference"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonRight);

    const FGameplayTag AimingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Status.Aiming")));
    AbilitySystem->AddLooseGameplayTag(AimingTag);
    PerspectiveComponent->ServerRequestPerspective(EUrbanPerspective::FirstPerson);
    TestEqual(TEXT("aiming rejects a perspective request"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonRight);
    AbilitySystem->RemoveLooseGameplayTag(AimingTag);
    TestEqual(TEXT("rejected request is not replayed after aiming clears"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonRight);

    PerspectiveComponent->RequestToggleShoulder();
    TestEqual(TEXT("third person can switch shoulders"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::ThirdPersonLeft);
    World->Tick(LEVELTICK_All, 0.25f);
    PerspectiveComponent->ServerRequestPerspective(EUrbanPerspective::FirstPerson);
    PerspectiveComponent->RequestToggleShoulder();
    TestEqual(TEXT("first person ignores shoulder requests"), PerspectiveComponent->GetAcceptedPerspective(), EUrbanPerspective::FirstPerson);

    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanPerspectivePresentationVisibilityTest,
    "UrbanSpear.CharacterCamera.Presentation.OwnerVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanPerspectivePresentationVisibilityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveComponentTests;

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    APawn* Pawn = World->SpawnActor<APawn>();
    ULyraAbilitySystemComponent* AbilitySystem = NewObject<ULyraAbilitySystemComponent>(Pawn);
    UUrbanCharacterStateComponent* StateComponent = NewObject<UUrbanCharacterStateComponent>(Pawn);
    UUrbanViewPolicyComponent* PolicyComponent = NewObject<UUrbanViewPolicyComponent>(Pawn);
    UUrbanPerspectiveComponent* PerspectiveComponent = NewObject<UUrbanPerspectiveComponent>(Pawn);
    UUrbanPerspectivePresentationComponent* PresentationComponent = NewObject<UUrbanPerspectivePresentationComponent>(Pawn);
    ULyraPawnComponent_CharacterParts* CharacterPartsComponent = NewObject<ULyraPawnComponent_CharacterParts>(Pawn);
    USkeletalMeshComponent* WorldBody = NewObject<USkeletalMeshComponent>(Pawn);
    USkeletalMeshComponent* CameraOccludingUpperBody = NewObject<USkeletalMeshComponent>(Pawn);
    USkeletalMeshComponent* FirstPersonArms = NewObject<USkeletalMeshComponent>(Pawn);
    UStaticMeshComponent* FirstPersonWeapon = NewObject<UStaticMeshComponent>(Pawn);

    WorldBody->ComponentTags.Add(FName(TEXT("Urban.WorldBody")));
    CameraOccludingUpperBody->ComponentTags.Add(FName(TEXT("Urban.FirstPersonBodyHidden")));
    FirstPersonArms->ComponentTags.Add(FName(TEXT("Urban.FirstPersonArms")));
    FirstPersonWeapon->ComponentTags.Add(FName(TEXT("Urban.FirstPersonWeapon")));
    WorldBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    WorldBody->SetCastShadow(true);
    WorldBody->SetIsReplicated(true);

    UActorComponent* Components[] = {
        AbilitySystem,
        StateComponent,
        PolicyComponent,
        PerspectiveComponent,
        PresentationComponent,
        CharacterPartsComponent,
        WorldBody,
        CameraOccludingUpperBody,
        FirstPersonArms,
        FirstPersonWeapon,
    };
    for (UActorComponent* Component : Components)
    {
        AttachComponent(Pawn, Component);
        Component->RegisterComponent();
    }
    Pawn->SetRootComponent(WorldBody);

    UChildActorComponent* CosmeticPartComponent = NewObject<UChildActorComponent>(Pawn);
    AttachComponent(Pawn, CosmeticPartComponent);
    CosmeticPartComponent->SetupAttachment(WorldBody);
    CosmeticPartComponent->SetChildActorClass(AStaticMeshActor::StaticClass());
    CosmeticPartComponent->RegisterComponent();
    AStaticMeshActor* CosmeticActor = CastChecked<AStaticMeshActor>(CosmeticPartComponent->GetChildActor());
    UStaticMeshComponent* CosmeticBody = CosmeticActor->GetStaticMeshComponent();

    UChildActorComponent* VisibleBodyPartComponent = NewObject<UChildActorComponent>(Pawn);
    AttachComponent(Pawn, VisibleBodyPartComponent);
    VisibleBodyPartComponent->SetupAttachment(WorldBody);
    VisibleBodyPartComponent->SetChildActorClass(AStaticMeshActor::StaticClass());
    VisibleBodyPartComponent->RegisterComponent();
    AStaticMeshActor* VisibleBodyActor = CastChecked<AStaticMeshActor>(VisibleBodyPartComponent->GetChildActor());
    UStaticMeshComponent* VisibleBodyPart = VisibleBodyActor->GetStaticMeshComponent();
    VisibleBodyPart->ComponentTags.Add(FName(TEXT("Urban.FirstPersonBodyVisible")));

    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    Pawn->DispatchBeginPlay();

    TestFalse(TEXT("first person keeps the world body visible to preserve legs and necessary body parts"), WorldBody->bOwnerNoSee);
    TestFalse(TEXT("first person keeps explicitly visible body parts visible to their owner"), VisibleBodyPart->bOwnerNoSee);
    TestTrue(TEXT("first person hides explicitly camera-occluding upper-body parts"), CameraOccludingUpperBody->bOwnerNoSee);
    TestTrue(TEXT("first person hides unclassified full-body cosmetics from their owner"), CosmeticBody->bOwnerNoSee);
    TestTrue(
        TEXT("dynamic cosmetic body parts recognize the pawn as a visibility owner"),
        UPrimitiveComponentUtilities::GetVisibilityOwners(CosmeticBody).Contains(Pawn));
    TestTrue(TEXT("first-person arms are restricted to the owner"), FirstPersonArms->bOnlyOwnerSee);
    TestFalse(TEXT("first-person arms are visible to the owner in first person"), FirstPersonArms->bOwnerNoSee);
    TestTrue(TEXT("first-person weapon is restricted to the owner"), FirstPersonWeapon->bOnlyOwnerSee);
    TestFalse(TEXT("first-person weapon is visible to the owner in first person"), FirstPersonWeapon->bOwnerNoSee);
    TestEqual(TEXT("presentation does not disable world-body collision"), WorldBody->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
    TestTrue(TEXT("presentation preserves world-body shadow casting"), WorldBody->CastShadow);
    TestTrue(TEXT("presentation preserves world-body replication"), WorldBody->GetIsReplicated());

    UChildActorComponent* LateCosmeticPartComponent = NewObject<UChildActorComponent>(Pawn);
    AttachComponent(Pawn, LateCosmeticPartComponent);
    LateCosmeticPartComponent->SetupAttachment(WorldBody);
    LateCosmeticPartComponent->SetChildActorClass(AStaticMeshActor::StaticClass());
    LateCosmeticPartComponent->RegisterComponent();
    AStaticMeshActor* LateCosmeticActor = CastChecked<AStaticMeshActor>(LateCosmeticPartComponent->GetChildActor());
    UStaticMeshComponent* LateCosmeticBody = LateCosmeticActor->GetStaticMeshComponent();
    TestFalse(TEXT("late cosmetic body starts without first-person visibility applied"), LateCosmeticBody->bOwnerNoSee);

    CharacterPartsComponent->OnCharacterPartsChanged.Broadcast(CharacterPartsComponent);
    TestTrue(TEXT("character-parts change reapplies first-person visibility to late cosmetics"), LateCosmeticBody->bOwnerNoSee);

    PerspectiveComponent->ServerRequestPerspective(EUrbanPerspective::ThirdPersonRight);

    TestFalse(TEXT("third person keeps the world body visible for the owner"), WorldBody->bOwnerNoSee);
    TestFalse(TEXT("third person keeps explicitly visible body parts visible for the owner"), VisibleBodyPart->bOwnerNoSee);
    TestFalse(TEXT("third person restores explicitly hidden upper-body parts for the owner"), CameraOccludingUpperBody->bOwnerNoSee);
    TestFalse(TEXT("third person restores attached cosmetic body parts for the owner"), CosmeticBody->bOwnerNoSee);
    TestFalse(TEXT("third person restores late cosmetic body parts for the owner"), LateCosmeticBody->bOwnerNoSee);
    TestTrue(TEXT("third person hides first-person arms from the owner"), FirstPersonArms->bOwnerNoSee);
    TestTrue(TEXT("third person hides first-person weapon from the owner"), FirstPersonWeapon->bOwnerNoSee);
    TestTrue(TEXT("third-person transition keeps arms owner-only"), FirstPersonArms->bOnlyOwnerSee);
    TestTrue(TEXT("third-person transition keeps weapon owner-only"), FirstPersonWeapon->bOnlyOwnerSee);

    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUrbanMuzzleObstructionTraceTest,
    "UrbanSpear.CharacterCamera.Presentation.MuzzleObstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanMuzzleObstructionTraceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace UrbanPerspectiveComponentTests;

    AddExpectedError(
        TEXT("has no component tagged Urban.FirstPersonArms"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    APawn* Pawn = World->SpawnActor<APawn>();
    UUrbanPerspectivePresentationComponent* PresentationComponent = NewObject<UUrbanPerspectivePresentationComponent>(Pawn);
    UBoxComponent* OwnerCollider = NewObject<UBoxComponent>(Pawn);
    OwnerCollider->SetBoxExtent(FVector(20.0, 20.0, 20.0));
    OwnerCollider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    OwnerCollider->SetCollisionResponseToAllChannels(ECR_Block);
    AttachComponent(Pawn, OwnerCollider);
    OwnerCollider->RegisterComponent();
    Pawn->SetRootComponent(OwnerCollider);
    AttachComponent(Pawn, PresentationComponent);
    PresentationComponent->RegisterComponent();

    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* WallCollider = NewObject<UBoxComponent>(Wall);
    WallCollider->SetBoxExtent(FVector(10.0, 100.0, 100.0));
    WallCollider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    WallCollider->SetCollisionResponseToAllChannels(ECR_Block);
    Wall->SetRootComponent(WallCollider);
    Wall->AddInstanceComponent(WallCollider);
    WallCollider->RegisterComponent();
    Wall->SetActorLocation(FVector(100.0, 0.0, 0.0));

    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    Pawn->DispatchBeginPlay();
    Wall->DispatchBeginPlay();
    PresentationComponent->RefreshPresentation();
    PresentationComponent->RefreshPresentation();

    const FUrbanMuzzleObstructionResult Result = PresentationComponent->TraceMuzzleToAim(
        FVector::ZeroVector,
        FVector(200.0, 0.0, 0.0));

    TestTrue(TEXT("wall obstructs muzzle-to-aim visibility trace"), Result.bObstructed);
    TestEqual(TEXT("obstruction reports the wall rather than the ignored owner"), Result.HitActor.Get(), Wall);
    TestTrue(TEXT("obstruction impact lies on the wall"), FMath::IsNearlyEqual(Result.ImpactPoint.X, 90.0, 1.0));

    const FUrbanMuzzleObstructionResult ClearResult = PresentationComponent->TraceMuzzleToAim(
        FVector::ZeroVector,
        FVector(0.0, 200.0, 0.0));
    TestFalse(TEXT("clear muzzle-to-aim path is not obstructed"), ClearResult.bObstructed);
    TestNull(TEXT("clear trace has no hit actor"), ClearResult.HitActor.Get());

    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);

    return true;
}

#endif
