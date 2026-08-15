// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/UrbanSmokeCloud.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "LyraLogChannels.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UrbanSmokeCloud)

namespace UrbanSmokeCloudAssets
{
	// Engine basic sphere + the engine's additive unlit emissive material.
	// EmissiveMeshMaterial exposes an "EmissiveColor" vector parameter and
	// renders regardless of lighting (BLEND_Additive + MSM_Unlit), which makes
	// it a reliable smoke puff: overlapping spheres sum to a soft cloud.
	// Hard-referenced so the cooker stages them into the pak.
	static const TCHAR* SphereMesh = TEXT(
		"/Engine/BasicShapes/Sphere.Sphere");
	static const TCHAR* EmissiveMaterial = TEXT(
		"/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial");
}

AUrbanSmokeCloud::AUrbanSmokeCloud(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// The occlusion volume: collision is disabled (bots query it manually),
	// but it doubles as a clear debug primitive in the editor.
	OcclusionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OcclusionSphere"));
	OcclusionSphere->SetupAttachment(Root);
	OcclusionSphere->SetSphereRadius(OcclusionRadius);
	OcclusionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OcclusionSphere->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(UrbanSmokeCloudAssets::SphereMesh);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EmissiveMaterialFinder(UrbanSmokeCloudAssets::EmissiveMaterial);

	SphereMeshRadius = SphereMeshFinder.Succeeded() ? SphereMeshFinder.Object->GetBounds().SphereRadius : 50.0f;
	if (SphereMeshRadius <= 0.0f)
	{
		SphereMeshRadius = 50.0f;
	}

	// Overlapping sphere layout: one central sphere plus four satellites
	// offset in X/Y/Z. The additive blend sums them into a lumpy cloud with a
	// brighter core and softer edges than a single solid sphere.
	const float SatOffset = 250.0f;
	SphereOffsets = {
		FVector(0.0f, 0.0f, 0.0f),
		FVector(SatOffset, 0.0f, 60.0f),
		FVector(-SatOffset, 0.0f, 60.0f),
		FVector(0.0f, SatOffset, -60.0f),
		FVector(0.0f, -SatOffset, -60.0f),
		FVector(0.0f, 0.0f, 140.0f),
	};
	SphereSizeFractions = {
		0.70f, 0.45f, 0.45f, 0.45f, 0.45f, 0.50f,
	};

	for (int32 Index = 0; Index < SphereOffsets.Num(); ++Index)
	{
		UStaticMeshComponent* Sphere = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("SmokeSphere%d"), Index));
		Sphere->SetupAttachment(Root);
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (SphereMeshFinder.Succeeded())
		{
			Sphere->SetStaticMesh(SphereMeshFinder.Object);
		}
		if (EmissiveMaterialFinder.Succeeded())
		{
			Sphere->SetMaterial(0, EmissiveMaterialFinder.Object);
		}

		Sphere->SetRelativeLocation(SphereOffsets[Index]);
		Sphere->SetRelativeScale3D(FVector(0.01f)); // start tiny, grow in BeginPlay

		SmokeSpheres.Add(Sphere);
	}
}

void AUrbanSmokeCloud::BeginPlay()
{
	Super::BeginPlay();

	UpdateVisual();
}

void AUrbanSmokeCloud::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;

	// Grow in, hold, then fade out.
	if (Age < GrowTime)
	{
		CurrentAlpha = FMath::GetMappedRangeValueClamped(
			FVector2D(0.0f, GrowTime), FVector2D(0.0f, 1.0f), Age);
	}
	else if (Age < Lifetime - FadeTime)
	{
		CurrentAlpha = 1.0f;
	}
	else
	{
		CurrentAlpha = FMath::GetMappedRangeValueClamped(
			FVector2D(Lifetime - FadeTime, Lifetime), FVector2D(1.0f, 0.0f), Age);
	}

	UpdateVisual();

	if (Age >= Lifetime)
	{
		Destroy();
	}
}

void AUrbanSmokeCloud::UpdateVisual()
{
	const float Alpha = FMath::Max(CurrentAlpha, 0.01f);

	for (int32 Index = 0; Index < SmokeSpheres.Num(); ++Index)
	{
		UStaticMeshComponent* Sphere = SmokeSpheres[Index];
		if (!Sphere)
		{
			continue;
		}

		// Target world radius = cloud radius * this sphere's size fraction;
		// divide by the mesh's native radius to get the required scale.
		const float TargetRadius = OcclusionRadius * SphereSizeFractions[Index];
		const float Scale = (TargetRadius / SphereMeshRadius) * Alpha;
		Sphere->SetRelativeScale3D(FVector(Scale));

		// The additive material renders its emissive color directly; alpha is
		// baked into the color so the cloud dims as it fades.
		UMaterialInstanceDynamic* MID = Sphere->CreateAndSetMaterialInstanceDynamic(0);
		if (MID)
		{
			// Grey-white smoke; keep peak brightness modest so overlapping
			// additive spheres don't blow out to pure white.
			MID->SetVectorParameterValue(TEXT("EmissiveColor"),
				FLinearColor(0.30f, 0.30f, 0.32f) * Alpha);
		}
	}
}
