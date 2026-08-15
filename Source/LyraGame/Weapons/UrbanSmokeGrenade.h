// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "UrbanSmokeGrenade.generated.h"

#define UE_API LYRAGAME_API

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * Throwable smoke grenade (projectile).
 *
 * Mirrors the stock B_Grenade in behavior (bounce + fuse timer) but has no
 * damage: when the fuse expires it spawns an AUrbanSmokeCloud and destroys
 * itself. Built entirely in C++ so no editor-authored blueprint is required;
 * the smoke mesh and fog visuals are hard-referenced so they survive cooking.
 */
UCLASS(MinimalAPI)
class AUrbanSmokeGrenade : public AActor
{
	GENERATED_BODY()

public:
	UE_API AUrbanSmokeGrenade(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the projectile movement component so throwers can set velocity. */
	UE_API UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }

protected:
	//~ Begin AActor
	UE_API virtual void BeginPlay() override;
	//~ End AActor

	/** Seconds from spawn to detonation. */
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Smoke")
	float FuseTime = 2.0f;

	/** The cloud class to spawn on detonation. */
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Smoke")
	TSubclassOf<AActor> SmokeCloudClass;

private:
	void Detonate();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	FTimerHandle FuseTimerHandle;
};

#undef UE_API
