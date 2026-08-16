// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "UrbanSmokeCloud.generated.h"

#define UE_API LYRAGAME_API

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * A short-lived smoke cloud spawned when a smoke grenade detonates.
 *
 * Pure visual + tactical occlusion: the actor carries an invisible sphere
 * whose radius bots query (see UUrbanSimpleBotComponent::FindVisibleEnemy) to
 * decide whether an enemy is hidden behind smoke. The visible smoke is a
 * cluster of overlapping additive-blended spheres (engine BasicShapes Sphere +
 * EmissiveMeshMaterial) that billow out, hold, then fade and are destroyed.
 * No damage, no GAS involvement - it is purely a line-of-sight blocker.
 */
UCLASS(MinimalAPI)
class AUrbanSmokeCloud : public AActor
{
	GENERATED_BODY()

public:
	UE_API AUrbanSmokeCloud(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Resolves the normalized visual density for a lifecycle age. */
	UE_API static float ResolveLifecycleAlpha(float Age, float GrowTime, float Lifetime, float FadeTime);

	/** Radius (units) within which this cloud blocks bot line of sight. */
	UE_API float GetCloudRadius() const { return OcclusionRadius; }
	UE_API float GetLifetime() const { return Lifetime; }
	UE_API float GetGrowTime() const { return GrowTime; }
	UE_API float GetFadeTime() const { return FadeTime; }

	//~ Begin AActor
	UE_API virtual void Tick(float DeltaSeconds) override;
	//~ End AActor

protected:
	//~ Begin AActor
	UE_API virtual void BeginPlay() override;
	//~ End AActor

	/** Occlusion radius in world units. */
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Smoke")
	float OcclusionRadius = 600.0f;

	/** Total lifetime before the cloud is destroyed. */
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Smoke")
	float Lifetime = 8.0f;

	/** How long (seconds) the cloud takes to grow to full size. */
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Smoke")
	float GrowTime = 1.5f;

	/** How long (seconds) the cloud takes to fade out at the end. */
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Smoke")
	float FadeTime = 2.0f;

private:
	/** Applies the current alpha to every sphere's scale and emissive color. */
	void UpdateVisual();

	UPROPERTY()
	TObjectPtr<USphereComponent> OcclusionSphere;

	/** The overlapping additive spheres that make up the visible cloud. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SmokeSpheres;

	/** Base (unscaled) radius of the engine sphere mesh, for scale math. */
	float SphereMeshRadius = 50.0f;

	/** Each sphere's relative offset and size fraction of the cloud radius. */
	TArray<FVector> SphereOffsets;
	TArray<float> SphereSizeFractions;

	float Age = 0.0f;
	float CurrentAlpha = 0.0f;
};

#undef UE_API
