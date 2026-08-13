// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/UrbanSmokeGrenade.h"

#include "Weapons/UrbanSmokeCloud.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "LyraLogChannels.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UrbanSmokeGrenade)

namespace UrbanSmokeGrenadeAssets
{
	// Reuse the stock grenade mesh so the smoke grenade reads as a thrown
	// grenade. Hard-referenced to survive cooking.
	static const TCHAR* GrenadeMesh = TEXT(
		"/Game/Weapons/Grenade/Mesh/SM_grenade.SM_grenade");
}

AUrbanSmokeGrenade::AUrbanSmokeGrenade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->SetSphereRadius(12.0f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionSphere->SetSimulatePhysics(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(CollisionSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(UrbanSmokeGrenadeAssets::GrenadeMesh);
	if (MeshFinder.Succeeded())
	{
		Mesh->SetStaticMesh(MeshFinder.Object);
	}

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 2200.0f;
	ProjectileMovement->MaxSpeed = 2200.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.4f;
	ProjectileMovement->Friction = 0.1f;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 20.0f;

	// Default to our own cloud class; can be overridden in a subclass.
	SmokeCloudClass = AUrbanSmokeCloud::StaticClass();

	// Thrown by a pawn; no network replication needed for the prototype.
	SetReplicates(false);
}

void AUrbanSmokeGrenade::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogLyra, Log, TEXT("AUrbanSmokeGrenade::BeginPlay - fuse %.1fs, will spawn cloud at %s."),
		FuseTime, *GetActorLocation().ToString());

	GetWorldTimerManager().SetTimer(FuseTimerHandle, this, &AUrbanSmokeGrenade::Detonate, FuseTime, false);
}

void AUrbanSmokeGrenade::Detonate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogLyra, Log, TEXT("AUrbanSmokeGrenade::Detonate at %s - spawning cloud class %s."),
		*GetActorLocation().ToString(),
		SmokeCloudClass ? *SmokeCloudClass->GetName() : TEXT("NULL"));

	if (SmokeCloudClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = GetInstigator();
		World->SpawnActor<AActor>(SmokeCloudClass, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	}

	Destroy();
}
