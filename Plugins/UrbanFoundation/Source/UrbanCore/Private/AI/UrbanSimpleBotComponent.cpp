#include "AI/UrbanSimpleBotComponent.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "AIController.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Equipment/LyraEquipmentDefinition.h"
#include "Equipment/LyraEquipmentInstance.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameModes/LyraGameMode.h"
#include "LyraGameplayTags.h"
#include "Player/LyraPlayerBotController.h"
#include "Player/LyraPlayerState.h"
#include "Teams/LyraTeamAgentInterface.h"
#include "Weapons/LyraRangedWeaponInstance.h"
#include "Weapons/UrbanSmokeCloud.h"
#include "Weapons/UrbanSmokeGrenade.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SoftObjectPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UrbanSimpleBotComponent)

DEFINE_LOG_CATEGORY_STATIC(LogUrbanSimpleBot, Log, All);

UUrbanSimpleBotComponent::UUrbanSimpleBotComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UUrbanSimpleBotComponent::BeginPlay()
{
	Super::BeginPlay();
}

AAIController* UUrbanSimpleBotComponent::GetBotController() const
{
	return Cast<AAIController>(GetOwner());
}

void UUrbanSimpleBotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AAIController* BotController = GetBotController();
	if (!BotController)
	{
		return;
	}
	APawn* Pawn = BotController->GetPawn();
	if (!Pawn)
	{
		// The bot's pawn died or was destroyed. The controller (and its
		// player state/team) survives, so respawn a fresh pawn after a short
		// delay to keep the arena populated.
		HandleRespawn(DeltaTime);
		return;
	}

	// A dead/dying pawn can still be possessed for a few frames while the
	// death animation plays. Stop all combat while dying so a bot that has
	// just been killed can't keep shooting the player.
	if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn))
	{
		if (Health->IsDeadOrDying())
		{
			LockedTarget = nullptr;
			FireReleased();
			return;
		}
	}

	// Reload timer: once it elapses the magazine is refilled and the bot can
	// fire again (the reload animation plays while this counts down).
	if (ReloadTimer > 0.0f)
	{
		ReloadTimer -= DeltaTime;
		if (ReloadTimer <= 0.0f)
			{
				AmmoInMagazine = GetMagazineSize();
			}
	}

	// Vision first: is there a visible enemy ahead?
	FVector EnemyDirection;
	AActor* Enemy = nullptr;
	if (FindVisibleEnemy(EnemyDirection, Enemy))
	{
		LockedTarget = Enemy;
		DecideAndAct(DeltaTime, EnemyDirection, Enemy);
	}
	else
	{
		LockedTarget = nullptr;
		FireReleased();                 // stop shooting when nothing to shoot
		Patrol(DeltaTime);              // 6. wander when no enemy around
	}
}

void UUrbanSimpleBotComponent::HandleRespawn(float DeltaTime)
{
	// Prime the countdown on the first frame without a pawn, then wait out
	// the delay before asking the game mode to restart the bot.
	if (RespawnTimer <= 0.0f)
	{
		RespawnTimer = RespawnDelay;
	}
	RespawnTimer -= DeltaTime;
	if (RespawnTimer > 0.0f)
	{
		return;
	}

	AAIController* BotController = GetBotController();
	UWorld* World = GetWorld();
	if (!BotController || !World)
	{
		return;
	}

	ALyraGameMode* GameMode = Cast<ALyraGameMode>(World->GetAuthGameMode());
	if (!GameMode)
	{
		UE_LOG(LogUrbanSimpleBot, Warning, TEXT("HandleRespawn: no Lyra game mode to respawn %s through."),
			*GetNameSafe(BotController));
		return;
	}

	// Match the full initialization path used by SpawnBots: the game mode's
	// generic initialization broadcasts OnGameModePlayerInitialized (the hook
	// that marks a controller as a fully-fledged participant) and is required
	// for the respawned pawn's movement to activate. Respawn without it yields
	// a pawn that spawns but never moves.
	//
	// The pawn's death leaves bIgnoreMoveInput set (the death/restart flow
	// ignores movement input while dead), and only this controller call
	// re-enables it - exactly what ALyraPlayerBotController::ServerRestartController
	// does before restarting. Without it the respawned pawn stands still even
	// though everything else (possess, movement component, walk speed) is fine.
	BotController->ResetIgnoreInputFlags();
	GameMode->GenericPlayerInitialization(BotController);
	GameMode->RestartPlayer(BotController);

	if (APawn* NewPawn = BotController->GetPawn())
	{
		if (ULyraPawnExtensionComponent* PawnExtComponent =
				NewPawn->FindComponentByClass<ULyraPawnExtensionComponent>())
		{
			PawnExtComponent->CheckDefaultInitialization();
		}

		EnsureVisibleMesh(NewPawn);
		EquipStarterWeapon(NewPawn);
		AmmoInMagazine = GetMagazineSize();
		ReloadTimer = 0.0f;
	}
	else
	{
		UE_LOG(LogUrbanSimpleBot, Warning, TEXT("HandleRespawn: respawn produced no pawn for %s."),
			*GetNameSafe(BotController));
	}
}

void UUrbanSimpleBotComponent::EnsureVisibleMesh(APawn* BotPawn)
{
	if (!BotPawn)
	{
		return;
	}

	// Idempotent: only swap the mesh once per pawn.
	static const FName VisibleMeshTag(TEXT("Urban.VisibleMeshApplied"));
	if (BotPawn->Tags.Contains(VisibleMeshTag))
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMesh = BotPawn->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkeletalMesh)
	{
		UE_LOG(LogUrbanSimpleBot, Warning, TEXT("EnsureVisibleMesh: %s has no skeletal mesh component."),
			*GetNameSafe(BotPawn));
		return;
	}

	// The character-camera milestone configures the hero body as an "Invis"
	// placeholder mesh so the first-person camera never sees the player's own
	// body. Bots must instead use the visible Mannequin mesh, or they are
	// completely invisible to the player.
	USkeletalMesh* VisibleMesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"));
	if (VisibleMesh)
	{
		SkeletalMesh->SetSkeletalMesh(VisibleMesh);
	}
	else
	{
		UE_LOG(LogUrbanSimpleBot, Warning, TEXT("EnsureVisibleMesh: failed to load SKM_Manny visible mesh."));
	}

	// Load the AnimBlueprint's generated class directly. In a cooked/packaged
	// build the blueprint asset object is not a standalone loadable object -
	// only its generated class (*_C) is cooked - so loading the asset itself
	// fails with "未找到Object" while loading the class works. Same pattern the
	// weapon anim-layer linking below already uses.
	UClass* AnimBPClass = LoadObject<UClass>(nullptr,
		TEXT("/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base_C"));
	if (AnimBPClass)
	{
		SkeletalMesh->SetAnimInstanceClass(AnimBPClass);
	}
	else
	{
		UE_LOG(LogUrbanSimpleBot, Warning, TEXT("EnsureVisibleMesh: failed to load ABP_Mannequin_Base animation."));
	}

	// Team tint: the Mannequin material exposes a TeamColor vector parameter.
	// Red squad (team 1) and blue squad (team 2) get distinct bodies so the
	// two sides are visually identifiable (otherwise every bot wears the
	// default grey-blue Manny material and the teams look identical).
	if (const ALyraPlayerState* PS = Cast<ALyraPlayerState>(BotPawn->GetPlayerState()))
	{
		const int32 TeamId = PS->GetTeamId();
		if (TeamId == 1 || TeamId == 2)
		{
			const FLinearColor TeamColor = (TeamId == 1)
				? FLinearColor(0.85f, 0.12f, 0.12f, 1.0f)  // red
				: FLinearColor(0.12f, 0.35f, 0.95f, 1.0f); // blue
			for (int32 Index = 0; Index < SkeletalMesh->GetNumMaterials(); ++Index)
			{
				if (UMaterialInstanceDynamic* MID = SkeletalMesh->CreateAndSetMaterialInstanceDynamic(Index))
				{
					MID->SetVectorParameterValue(TEXT("TeamColor"), TeamColor);
				}
			}
		}
	}

	BotPawn->Tags.Add(VisibleMeshTag);
}

void UUrbanSimpleBotComponent::EquipStarterWeapon(APawn* BotPawn)
{
	if (!BotPawn)
	{
		return;
	}

	ULyraEquipmentManagerComponent* EquipmentManager =
		BotPawn->FindComponentByClass<ULyraEquipmentManagerComponent>();
	if (!EquipmentManager)
	{
		UE_LOG(LogUrbanSimpleBot, Warning, TEXT("EquipStarterWeapon: %s has no equipment manager."),
			*GetNameSafe(BotPawn));
		return;
	}

	// The human player gets weapons through the weapon-selection UI, which
	// slots an item into the quick bar and activates it. Bots have no UI, so
	// equip the weapon matching the bot's WeaponType directly on the equipment
	// manager. Equipping the real weapon definition spawns the weapon actor
	// (B_Pistol/B_Rifle/B_Shotgun) attached to the hand socket, so the bot
	// visibly carries the same gun it fires.
	const TCHAR* EquipmentPath = nullptr;
	const TCHAR* ItemPath = nullptr;
	switch (WeaponType)
	{
	case EBotWeaponType::Rifle:
		EquipmentPath = TEXT("/ShooterCore/Weapons/Rifle/WID_Rifle.WID_Rifle_C");
		ItemPath = TEXT("/ShooterCore/Weapons/Rifle/ID_Rifle.ID_Rifle_C");
		break;
	case EBotWeaponType::Shotgun:
		EquipmentPath = TEXT("/ShooterCore/Weapons/Shotgun/WID_Shotgun.WID_Shotgun_C");
		ItemPath = TEXT("/ShooterCore/Weapons/Shotgun/ID_Shotgun.ID_Shotgun_C");
		break;
	case EBotWeaponType::Pistol:
	default:
		EquipmentPath = TEXT("/ShooterCore/Weapons/Pistol/WID_Pistol.WID_Pistol_C");
		ItemPath = TEXT("/ShooterCore/Weapons/Pistol/ID_Pistol.ID_Pistol_C");
		break;
	}

	TSubclassOf<ULyraEquipmentDefinition> WeaponClass = TSoftClassPtr<ULyraEquipmentDefinition>(
		FSoftObjectPath(EquipmentPath)).LoadSynchronous();
	if (!WeaponClass)
	{
		UE_LOG(LogUrbanSimpleBot, Warning, TEXT("EquipStarterWeapon: failed to load weapon equipment definition (%s)."),
			EquipmentPath);
		return;
	}

	if (ULyraEquipmentInstance* Equipment = EquipmentManager->EquipItem(WeaponClass))
	{
		// The weapon-fire ability's cost checks the magazine-ammo stack on the
		// equipment's *associated item* (the player's quick-bar item carries
		// the ammo; a directly equipped weapon has no item). Give the bot an
		// item instance with a full magazine so the fire ability can activate.
		TSubclassOf<ULyraInventoryItemDefinition> ItemDef = TSoftClassPtr<ULyraInventoryItemDefinition>(
			FSoftObjectPath(ItemPath)).LoadSynchronous();
		if (ItemDef)
		{
			ULyraInventoryItemInstance* ItemInstance = NewObject<ULyraInventoryItemInstance>(BotPawn);
			ItemInstance->AddStatTagStack(
				FGameplayTag::RequestGameplayTag(FName("Lyra.ShooterGame.Weapon.MagazineAmmo")),
				999);
			Equipment->SetInstigator(ItemInstance);
		}
		else
		{
			UE_LOG(LogUrbanSimpleBot, Warning, TEXT("EquipStarterWeapon: failed to load item definition for ammo (%s)."),
				ItemPath);
		}

		// Link the weapon's animation layer to the bot's mesh. The fire/reload
		// montages play on the FullBodyAdditivePreAim slot, which is provided
		// by the weapon animation layer (ABP_*AnimLayers) via the
		// ALI_ItemAnimLayers interface - without linking it, the montage plays
		// but produces no visible pose (this is why bots had no fire anim).
		const TCHAR* AnimLayerPath = nullptr;
		switch (WeaponType)
		{
		case EBotWeaponType::Rifle:
			AnimLayerPath = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/ABP_RifleAnimLayers.ABP_RifleAnimLayers_C");
			break;
		case EBotWeaponType::Shotgun:
			AnimLayerPath = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Shotgun/ABP_ShotgunAnimLayers.ABP_ShotgunAnimLayers_C");
			break;
		case EBotWeaponType::Pistol:
		default:
			AnimLayerPath = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Pistol/ABP_PistolAnimLayers.ABP_PistolAnimLayers_C");
			break;
		}
		if (UClass* AnimLayerClass = LoadObject<UClass>(nullptr, AnimLayerPath))
		{
			if (USkeletalMeshComponent* Mesh = BotPawn->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
				{
					AnimInstance->LinkAnimClassLayers(AnimLayerClass);
				}
			}
		}
		else
		{
			UE_LOG(LogUrbanSimpleBot, Warning, TEXT("EquipStarterWeapon: failed to load anim layer %s."), AnimLayerPath);
		}
	}
}

// ---------------------------------------------------------------------------
// 1. Movement module
// ---------------------------------------------------------------------------
void UUrbanSimpleBotComponent::MoveInDirection(const FVector& WorldDirection, float SpeedScale)
{
	AAIController* BotController = GetBotController();
	APawn* Pawn = BotController ? BotController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const FVector Dir = WorldDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero() || SpeedScale <= 0.0f)
	{
		Pawn->AddMovementInput(FVector::ZeroVector);
		return;
	}

	// Steer around obstacles: probe the path ahead and pick a clear sideways
	// direction so the bot follows walls and rounds corners instead of
	// grinding into them.
	const FVector SteeredDir = AdjustDirectionForObstacles(Pawn, Dir);

	Pawn->AddMovementInput(SteeredDir, SpeedScale);

	// Face the direction we are moving (only when not aiming - aiming turns
	// elsewhere via TurnAndShoot).
	if (!LockedTarget)
	{
		FaceDirection(SteeredDir);
	}
}

// ---------------------------------------------------------------------------
// 2. Shooting module
// ---------------------------------------------------------------------------
void UUrbanSimpleBotComponent::TurnAndShoot(const FVector& TargetDirection)
{
	FaceDirection(TargetDirection);

	// Press fire on a fixed cadence; release in between so semi-auto weapons
	// (pistol/shotgun) fire one shot per press, and auto weapons still cycle.
	FireTimer -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	if (FireTimer <= 0.0f)
	{
		if (NeedsReload())
		{
			// Empty mag: play the reload animation and hold fire until it is
			// done (the tick updates the timer and refills the mag). Drop the
			// aiming pose for the reload, which is played with the gun down.
			StartReload();
			SetAiming(false);
			FireTimer = 1.0f;
			return;
		}

		FireReleased();
		SetAiming(true); // raise the gun into the aiming pose for the shot
		FirePressed();
		FireTimer = GetShotInterval();
	}
}

void UUrbanSimpleBotComponent::FaceDirection(const FVector& WorldDirection)
{
	AAIController* BotController = GetBotController();
	APawn* Pawn = BotController ? BotController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const FVector Dir = WorldDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		return;
	}

	FRotator NewRotation = Dir.Rotation();
	NewRotation.Pitch = 0.0f;
	NewRotation.Roll = 0.0f;

	// Small-angle dead zone: only rotate when the new facing is meaningfully
	// different from the current one. Without this, the per-frame aim jitter
	// (the enemy's position shifts slightly each tick) makes the bot visibly
	// twitch in place while shooting or closing in.
	const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(Pawn->GetActorRotation().Yaw, NewRotation.Yaw));
	if (YawDelta < 5.0f)
	{
		return;
	}

	Pawn->SetActorRotation(NewRotation);

	// Rotate the controller (and thus the camera) to face the target too.
	// The weapon's targeting source is CameraTowardsFocus, so bullets fly
	// along the camera's view direction. Turning only the pawn's body leaves
	// the camera facing elsewhere and every shot misses - exactly what the
	// stock bots avoid by steering their controller with SetFocus.
	BotController->SetControlRotation(NewRotation);
}

FVector UUrbanSimpleBotComponent::AdjustDirectionForObstacles(const APawn* Pawn, const FVector& DesiredDir)
{
	if (!Pawn)
	{
		return DesiredDir;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return DesiredDir;
	}

	const FVector Eye = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, Pawn->BaseEyeHeight);
	const float ProbeDistance = 220.0f;  // how far ahead we look for walls
	const float ProbeRadius = 30.0f;     // capsule width of the check

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(UrbanBotAvoid), false, Pawn);
	QueryParams.AddIgnoredActor(Pawn);

	// Probe straight ahead first; if clear, keep the desired direction and
	// clear any latched side-bias (the obstacle is gone).
	FHitResult Hit;
	if (!World->SweepSingleByChannel(
			Hit, Eye, Eye + DesiredDir * ProbeDistance, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(ProbeRadius), QueryParams))
	{
		AvoidSideBias = 0;
		return DesiredDir;
	}

	// Blocked: pick the side once and stick with it so the bot doesn't flip
	// its steer direction every frame (which looks like jittering against the
	// wall). Probe that side first, then fall back to the other.
	if (AvoidSideBias == 0)
	{
		AvoidSideBias = FMath::RandBool() ? 1 : -1;
	}

	const int32 SideOrder[2] = { AvoidSideBias, -AvoidSideBias };
	for (const int32 Side : SideOrder)
	{
		for (int32 Step = 0; Step < 6; ++Step)
		{
			const float TurnDegrees = (30.0f + Step * 30.0f) * (Side > 0 ? 1.0f : -1.0f);
			const FVector Candidate = DesiredDir.RotateAngleAxis(TurnDegrees, FVector::UpVector);
			FHitResult Probe;
			if (!World->SweepSingleByChannel(
					Probe, Eye, Eye + Candidate * ProbeDistance, FQuat::Identity,
					ECC_Visibility, FCollisionShape::MakeSphere(ProbeRadius), QueryParams))
			{
				return Candidate.GetSafeNormal2D();
			}
		}
	}

	// Everything is blocked; reverse and try to back out.
	return (-DesiredDir).GetSafeNormal2D();
}

void UUrbanSimpleBotComponent::FirePressed()
{
	AAIController* BotController = GetBotController();
	APawn* Pawn = BotController ? BotController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return;
	}

	// Hit-scan fire along the bot's actual facing (control rotation). Each
	// weapon type has its own damage/range/pellet count; shotguns sweep
	// several slightly-spread pellets so a close hit lands multiple pellets.
	// Any enemy on the other team (player or bot) can be hit. Firing also
	// consumes one round of the magazine and plays the fire animation.
	if (NeedsReload())
	{
		return; // TurnAndShoot triggers reload; do not fire empty
	}

	const FVector Eye = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, Pawn->BaseEyeHeight);
	const FVector AimDir = BotController->GetControlRotation().Vector();
	const float Range = GetShotRange();
	const int32 Pellets = GetPellets();

	// Pick the fire GameplayCue for this weapon type. The cue spawns the
	// muzzle flash from the gun's weapon_r_muzzle socket and draws a tracer
	// from that muzzle to CueParams.Location - so Location MUST be the impact
	// point (or the far end of the trace), not the shooter's eye, otherwise
	// the tracer points the wrong way (e.g. down at the ground).
	static const FGameplayTag PistolFireCue = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.Pistol.Fire"));
	static const FGameplayTag RifleFireCue = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.Rifle.Fire"));
	static const FGameplayTag ShotgunFireCue = FGameplayTag::RequestGameplayTag(FName("GameplayCue.Weapon.Shotgun.Fire"));
	FGameplayTag FireCueTag = PistolFireCue;
	switch (WeaponType)
	{
	case EBotWeaponType::Rifle: FireCueTag = RifleFireCue; break;
	case EBotWeaponType::Shotgun: FireCueTag = ShotgunFireCue; break;
	case EBotWeaponType::Pistol:
	default: FireCueTag = PistolFireCue; break;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(UrbanBotFire), false, Pawn);
	QueryParams.AddIgnoredActor(Pawn);

	float TotalDamage = 0.0f;
	AActor* HitTarget = nullptr;
	bool bHitPawn = false;
	FVector TracerEnd = Eye + AimDir * Range; // default: trace runs to max range
	for (int32 PelletIndex = 0; PelletIndex < Pellets; ++PelletIndex)
	{
		FVector PelletDir = AimDir;
		if (Pellets > 1)
		{
			// Spread pellets around the aim direction (e.g. -4, 0, +4 degrees).
			const float SpreadDegrees = (PelletIndex - (Pellets - 1) * 0.5f) * 4.0f;
			PelletDir = AimDir.RotateAngleAxis(SpreadDegrees, Pawn->GetActorUpVector());
		}

		FHitResult Hit;
		if (!World->SweepSingleByChannel(
				Hit, Eye, Eye + PelletDir * Range, FQuat::Identity,
				ECC_Visibility, FCollisionShape::MakeSphere(15.0f), QueryParams))
		{
			continue;
		}

		AActor* HitActor = Hit.GetActor();
		if (!HitActor || !IsEnemy(HitActor))
		{
			// Impact on a wall: concrete dust so missed shots are visible too.
			static UNiagaraSystem* ImpactWallFX = nullptr;
			if (ImpactWallFX == nullptr)
			{
				ImpactWallFX = LoadObject<UNiagaraSystem>(nullptr,
					TEXT("/Game/Effects/Particles/Impacts/NS_ImpactConcrete.NS_ImpactConcrete"));
			}
			if (ImpactWallFX)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactWallFX, Hit.ImpactPoint);
			}
			continue;
		}
		// Impact on a pawn: character sparks, matching the player's hits.
		static UNiagaraSystem* ImpactPawnFX = nullptr;
		if (ImpactPawnFX == nullptr)
		{
			ImpactPawnFX = LoadObject<UNiagaraSystem>(nullptr,
				TEXT("/Game/Effects/Particles/Impacts/NS_ImactSparksCharacter.NS_ImactSparksCharacter"));
		}
		if (ImpactPawnFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactPawnFX, Hit.ImpactPoint);
		}
		HitTarget = HitActor;
		bHitPawn = true;
		TotalDamage += GetShotDamage();
		TracerEnd = Hit.ImpactPoint;
	}

	// Fire visual feedback: trigger the weapon's GameplayCue with the impact
	// point (or max-range point) as Location, so the muzzle flash spawns at
	// the gun and the tracer draws from the muzzle to the actual hit.
	FGameplayCueParameters CueParams;
	CueParams.Instigator = Pawn;
	CueParams.EffectCauser = Pawn;
	CueParams.Location = TracerEnd;
	if (ULyraAbilitySystemComponent* BotASC = Cast<ULyraAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)))
	{
		BotASC->ExecuteGameplayCue(FireCueTag, CueParams);
	}

	if (!HitTarget || TotalDamage <= 0.0f || !bHitPawn)
	{
		// A shot was fired (muzzle/tracer showed) even if nothing was hit;
		// still consume the round so empty-mag reloads behave normally.
		--AmmoInMagazine;
		PlayWeaponMontage(GetFireMontagePath());
		return;
	}

	// Apply damage through the victim's ability system so death, respawn and
	// the HUD all react normally. Players and bots both use ALyraPlayerState.
	const APawn* HitPawn = Cast<APawn>(HitTarget);
	if (!HitPawn)
	{
		return;
	}
	ALyraPlayerState* HitPS = Cast<ALyraPlayerState>(HitPawn->GetPlayerState());
	if (!HitPS)
	{
		return;
	}
	ULyraAbilitySystemComponent* TargetASC = HitPS->GetLyraAbilitySystemComponent();
	if (!TargetASC)
	{
		return;
	}

	UGameplayEffect* DamageGE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("UrbanBotDamage"));
	DamageGE->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Mod;
	Mod.Attribute = ULyraHealthSet::GetHealthAttribute();
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-TotalDamage));
	DamageGE->Modifiers.Add(Mod);

	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddInstigator(Pawn, Pawn);
	FGameplayEffectSpec DamageSpec(DamageGE, Context, 1.0f);
	TargetASC->ApplyGameplayEffectSpecToSelf(DamageSpec);

	// Consume one round and play the fire animation so the shot is visible.
	--AmmoInMagazine;
	PlayWeaponMontage(GetFireMontagePath());
}

void UUrbanSimpleBotComponent::FireReleased()
{
	// The event-driven fire ability is one shot per event; nothing to release.
	// Lower the gun again so the bot drops out of the aiming pose when it stops
	// shooting (patrolling, closing distance, retreating or dying).
	SetAiming(false);
}

void UUrbanSimpleBotComponent::SetAiming(bool bAiming)
{
	AAIController* BotController = GetBotController();
	APawn* Pawn = BotController ? BotController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC)
	{
		return;
	}

	// Same tag the player's ADS ability applies; the base Mannequin animation
	// blueprint maps it to GameplayTag_IsADS and transitions into the aiming
	// pose when it is present.
	static const FGameplayTag ADSTag = FGameplayTag::RequestGameplayTag(FName("Event.Movement.ADS"));

	if (bAiming)
	{
		if (!ASC->HasMatchingGameplayTag(ADSTag))
		{
			ASC->AddLooseGameplayTag(ADSTag);
		}
	}
	else if (ASC->HasMatchingGameplayTag(ADSTag))
	{
		ASC->RemoveLooseGameplayTag(ADSTag);
	}
}

void UUrbanSimpleBotComponent::ThrowGrenade()
{
	AAIController* BotController = GetBotController();
	APawn* Pawn = BotController ? BotController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return;
	}
	if (GrenadeCount <= 0)
	{
		return;
	}

	static const TCHAR* GrenadeClassPath = TEXT("/ShooterCore/Weapon/Grenade/B_Grenade.B_Grenade_C");
	UClass* GrenadeClass = LoadObject<UClass>(nullptr, GrenadeClassPath);
	if (!GrenadeClass)
	{
		return;
	}

	// Throw toward the current target (or straight ahead if none).
	FVector AimDir = BotController->GetControlRotation().Vector();
	if (LockedTarget != nullptr)
	{
		AimDir = (LockedTarget->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal();
	}
	AimDir.Z = 0.0f;
	AimDir = AimDir.GetSafeNormal();
	if (AimDir.IsNearlyZero())
	{
		AimDir = Pawn->GetActorForwardVector();
	}

	const FVector SpawnLocation = Pawn->GetActorLocation() + AimDir * 60.0f + FVector(0.0f, 0.0f, 60.0f);
	const FRotator ThrowRotation = AimDir.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Instigator = Pawn;
	SpawnParams.Owner = Pawn;
	AActor* Grenade = World->SpawnActor<AActor>(GrenadeClass, SpawnLocation, ThrowRotation, SpawnParams);
	if (Grenade)
	{
		if (UProjectileMovementComponent* ProjMove = Grenade->FindComponentByClass<UProjectileMovementComponent>())
		{
			const FVector ThrowVelocity = (AimDir + FVector(0.0f, 0.0f, 0.35f)).GetSafeNormal() * ProjMove->InitialSpeed;
			ProjMove->Velocity = ThrowVelocity;
		}
		--GrenadeCount;
	}
}

void UUrbanSimpleBotComponent::ThrowSmokeGrenade()
{
	AAIController* BotController = GetBotController();
	APawn* Pawn = BotController ? BotController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return;
	}
	if (SmokeGrenadeCount <= 0)
	{
		return;
	}

	// Throw toward the current target (or straight ahead if none) - same aim
	// as the frag grenade. The smoke detonates into a vision-blocking cloud.
	FVector AimDir = BotController->GetControlRotation().Vector();
	if (LockedTarget != nullptr)
	{
		AimDir = (LockedTarget->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal();
	}
	AimDir.Z = 0.0f;
	AimDir = AimDir.GetSafeNormal();
	if (AimDir.IsNearlyZero())
	{
		AimDir = Pawn->GetActorForwardVector();
	}

	const FVector SpawnLocation = Pawn->GetActorLocation() + AimDir * 60.0f + FVector(0.0f, 0.0f, 60.0f);
	const FRotator ThrowRotation = AimDir.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Instigator = Pawn;
	SpawnParams.Owner = Pawn;
	AUrbanSmokeGrenade* SmokeGrenade = World->SpawnActor<AUrbanSmokeGrenade>(
		AUrbanSmokeGrenade::StaticClass(), SpawnLocation, ThrowRotation, SpawnParams);
	if (SmokeGrenade)
	{
		if (UProjectileMovementComponent* ProjMove = SmokeGrenade->GetProjectileMovement())
		{
			const FVector ThrowVelocity = (AimDir + FVector(0.0f, 0.0f, 0.35f)).GetSafeNormal() * ProjMove->InitialSpeed;
			ProjMove->Velocity = ThrowVelocity;
		}
		--SmokeGrenadeCount;
	}
}

// ---------------------------------------------------------------------------
// 3. Vision module
// ---------------------------------------------------------------------------
bool UUrbanSimpleBotComponent::FindVisibleEnemy(FVector& OutDirection, AActor*& OutEnemy) const
{
	AAIController* BotController = GetBotController();
	APawn* Pawn = BotController ? BotController->GetPawn() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return false;
	}

	const FVector EyeLocation = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 160.0f);
	const FVector Forward = Pawn->GetActorForwardVector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDegrees));

	float BestDistance = TNumericLimits<float>::Max();
	AActor* BestEnemy = nullptr;

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!Candidate || Candidate == Pawn)
		{
			continue;
		}

		if (!IsEnemy(Candidate))
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - EyeLocation;
		const float Distance = ToCandidate.Size();
		if (Distance > SightRange || Distance < 50.0f)
		{
			continue;
		}

		const FVector Dir = ToCandidate / Distance;
		if (FVector::DotProduct(Forward, Dir) < MinDot)
		{
			continue; // outside the 120-degree cone
		}

		// Wall check: the line must reach the candidate unobstructed.
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(UrbanBotVision), false, Pawn);
		QueryParams.AddIgnoredActor(Pawn);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, EyeLocation, Candidate->GetActorLocation(), ECC_Visibility, QueryParams))
		{
			if (Hit.GetActor() != Candidate)
			{
				continue; // wall in the way
			}
		}

		// Smoke check: if the line of sight passes through a smoke cloud, the
		// enemy is hidden and cannot be locked or shot at. Smoke is purely a
		// vision blocker - bullets still pass through it.
		bool bBlockedBySmoke = false;
		for (TActorIterator<AUrbanSmokeCloud> SmokeIt(World); SmokeIt; ++SmokeIt)
		{
			const AUrbanSmokeCloud* Cloud = *SmokeIt;
			if (!Cloud)
			{
				continue;
			}

			const FVector CloudCenter = Cloud->GetActorLocation();
			const FVector ClosestPoint = FMath::ClosestPointOnSegment(CloudCenter, EyeLocation, Candidate->GetActorLocation());
			if (FVector::Dist(ClosestPoint, CloudCenter) < Cloud->GetCloudRadius())
			{
				bBlockedBySmoke = true;
				break;
			}
		}
		if (bBlockedBySmoke)
		{
			continue;
		}

		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestEnemy = Candidate;
		}
	}

	if (BestEnemy)
	{
		OutEnemy = BestEnemy;
		OutDirection = (BestEnemy->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
		return true;
	}

	return false;
}

bool UUrbanSimpleBotComponent::IsEnemy(const AActor* Other) const
{
	AAIController* BotController = GetBotController();
	if (!BotController || !Other)
	{
		return false;
	}

	// Red/Blue team fight: anyone on the other team is an enemy, whether that
	// is the human player or another bot. Both sides must have a team id
	// assigned (the spawner assigns bots; OnPlayerInitialized assigns the
	// player), otherwise the actor is treated as neutral.
	const ALyraPlayerState* MyPS = Cast<ALyraPlayerState>(BotController->PlayerState);
	const ALyraPlayerState* OtherPS = nullptr;
	if (const APawn* OtherPawn = Cast<const APawn>(Other))
	{
		OtherPS = Cast<ALyraPlayerState>(OtherPawn->GetPlayerState());
	}

	if (!MyPS || !OtherPS)
	{
		return false;
	}

	const int32 MyTeam = MyPS->GetTeamId();
	const int32 OtherTeam = OtherPS->GetTeamId();
	if (MyTeam == INDEX_NONE || OtherTeam == INDEX_NONE)
	{
		return false;
	}

	return MyTeam != OtherTeam;
}

// ---------------------------------------------------------------------------
// 4. Decision module
// ---------------------------------------------------------------------------
void UUrbanSimpleBotComponent::DecideAndAct(float DeltaTime, const FVector& EnemyDirection, AActor* Enemy)
{
	APawn* Pawn = GetBotController() ? GetBotController()->GetPawn() : nullptr;
	if (!Pawn || !Enemy)
	{
		return;
	}

	const float DistanceToEnemy = FVector::Dist(Pawn->GetActorLocation(), Enemy->GetActorLocation());
	const float PreferredDistance = GetPreferredEngageDistance();

	// Low health: retreat toward cover instead of trading shots. Back away
	// (and stop firing) until the enemy is far enough away or health recovers,
	// so a badly-wounded bot runs for a wall instead of standing and dying.
	bool bLowHealth = false;
	if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn))
	{
		bLowHealth = Health->GetHealthNormalized() < 0.4f;
	}
	const float CoverSafeDistance = FMath::Max(PreferredDistance, 1200.0f);
	if (bLowHealth && DistanceToEnemy < CoverSafeDistance)
	{
		// Pop a smoke grenade toward the enemy once per life to break line of
		// sight, then retreat - a wounded bot escapes behind its own cover.
		if (SmokeGrenadeCount > 0)
		{
			ThrowSmokeGrenade();
		}
		FleeFrom(Enemy->GetActorLocation());
		FireReleased();
		return;
	}

	// 5. Flee: enemy way too close - back away while still firing at it.
	if (DistanceToEnemy < FleeDistance)
	{
		FleeFrom(Enemy->GetActorLocation());
		TurnAndShoot(EnemyDirection);
		return;
	}

	// Throw a grenade when the enemy is at a good mid-range distance, so bots
	// use their limited stock of two grenades against a distant target.
	if (GrenadeCount > 0 && DistanceToEnemy > 600.0f && DistanceToEnemy < 1400.0f)
	{
		if (FMath::FRand() < 0.004f) // occasionally, not every frame
		{
			ThrowGrenade();
		}
	}

	if (HasWeapon())
	{
		// Weapon-based decision: move into the weapon's preferred engagement
		// distance, then strafe and shoot instead of standing still.
		const float MoveBand = PreferredDistance * 0.25f; // dead zone around ideal
		if (DistanceToEnemy > PreferredDistance + MoveBand)
		{
			// Too far: close the distance.
			FaceDirection(EnemyDirection);
			MoveInDirection(EnemyDirection);
			FireReleased();
		}
		else if (DistanceToEnemy < PreferredDistance - MoveBand)
		{
			// Too close: back off to the ideal range.
			FleeFrom(Enemy->GetActorLocation());
			TurnAndShoot(EnemyDirection);
		}
		else
		{
			// In range: strafe side to side while firing, so the bot is harder
			// to hit and looks more alive than a stationary turret.
			StrafeTimer -= DeltaTime;
			if (StrafeTimer <= 0.0f)
			{
				StrafeDir = -StrafeDir;
				StrafeTimer = FMath::FRandRange(1.0f, 2.0f);
			}
			const FVector StrafeDirection =
				(FVector::CrossProduct(FVector::UpVector, EnemyDirection) * StrafeDir).GetSafeNormal2D();
			TurnAndShoot(EnemyDirection);
			MoveInDirection(StrafeDirection, 0.6f);
		}
	}
	else
	{
		// No weapon: advance toward the enemy.
		FaceDirection(EnemyDirection);
		MoveInDirection(EnemyDirection);
		FireReleased();
	}
}

float UUrbanSimpleBotComponent::GetPreferredEngageDistance() const
{
	// Preferred engagement range per weapon type: shotguns are close-range,
	// pistols mid, rifles long.
	switch (WeaponType)
	{
	case EBotWeaponType::Shotgun:
		return 500.0f;
	case EBotWeaponType::Rifle:
		return 1300.0f;
	case EBotWeaponType::Pistol:
	default:
		return 800.0f;
	}
}

void UUrbanSimpleBotComponent::SetWeaponType(EBotWeaponType InType)
{
	WeaponType = InType;
	AmmoInMagazine = GetMagazineSize();
}
FUrbanBotWeaponProfile UUrbanSimpleBotComponent::GetWeaponProfile() const
{
	FUrbanBotWeaponProfile Profile;
	Profile.Damage = GetShotDamage();
	Profile.FireInterval = GetShotInterval();
	Profile.Range = GetShotRange();
	Profile.Pellets = GetPellets();
	Profile.MagazineSize = GetMagazineSize();
	return Profile;
}

int32 UUrbanSimpleBotComponent::GetMagazineSize() const
{
	switch (WeaponType)
	{
	case EBotWeaponType::Rifle:
		return 30;
	case EBotWeaponType::Shotgun:
		return 6;
	case EBotWeaponType::Pistol:
	default:
		return 12;
	}
}

const TCHAR* UUrbanSimpleBotComponent::GetFireMontagePath() const
{
	switch (WeaponType)
	{
	case EBotWeaponType::Rifle:
		return TEXT("/Game/Weapons/Rifle/Animations/AM_MM_Rifle_Fire.AM_MM_Rifle_Fire");
	case EBotWeaponType::Shotgun:
		return TEXT("/Game/Weapons/Shotgun/Animations/AM_MM_Shotgun_Fire.AM_MM_Shotgun_Fire");
	case EBotWeaponType::Pistol:
	default:
		return TEXT("/Game/Weapons/Pistol/Animations/AM_MM_Pistol_Fire.AM_MM_Pistol_Fire");
	}
}

const TCHAR* UUrbanSimpleBotComponent::GetReloadMontagePath() const
{
	switch (WeaponType)
	{
	case EBotWeaponType::Rifle:
		return TEXT("/Game/Weapons/Rifle/Animations/AM_MM_Rifle_Reload.AM_MM_Rifle_Reload");
	case EBotWeaponType::Shotgun:
		return TEXT("/Game/Weapons/Shotgun/Animations/AM_MM_Shotgun_Reload.AM_MM_Shotgun_Reload");
	case EBotWeaponType::Pistol:
	default:
		return TEXT("/Game/Weapons/Pistol/Animations/AM_MM_Pistol_Reload.AM_MM_Pistol_Reload");
	}
}

void UUrbanSimpleBotComponent::StartReload()
{
	if (ReloadTimer > 0.0f)
	{
		return; // already reloading
	}

	ReloadTimer = 1.8f;
	PlayWeaponMontage(GetReloadMontagePath());
}

void UUrbanSimpleBotComponent::PlayWeaponMontage(const TCHAR* MontagePath, float Rate)
{
	AAIController* BotController = GetBotController();
	APawn* Pawn = BotController ? BotController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh)
	{
		return;
	}

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, MontagePath);
	if (!Montage)
	{
		UE_LOG(LogUrbanSimpleBot, Warning, TEXT("PlayWeaponMontage: failed to load montage %s."), MontagePath);
		return;
	}

	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		AnimInstance->Montage_Play(Montage, Rate);
	}
}

float UUrbanSimpleBotComponent::GetShotDamage() const
{
	switch (WeaponType)
	{
	case EBotWeaponType::Rifle:
		return RifleDamage;
	case EBotWeaponType::Shotgun:
		return ShotgunDamage;
	case EBotWeaponType::Pistol:
	default:
		return PistolDamage;
	}
}

float UUrbanSimpleBotComponent::GetShotInterval() const
{
	switch (WeaponType)
	{
	case EBotWeaponType::Rifle:
		return RifleInterval;
	case EBotWeaponType::Shotgun:
		return ShotgunInterval;
	case EBotWeaponType::Pistol:
	default:
		return PistolInterval;
	}
}

float UUrbanSimpleBotComponent::GetShotRange() const
{
	switch (WeaponType)
	{
	case EBotWeaponType::Rifle:
		return RifleRange;
	case EBotWeaponType::Shotgun:
		return ShotgunRange;
	case EBotWeaponType::Pistol:
	default:
		return PistolRange;
	}
}

int32 UUrbanSimpleBotComponent::GetPellets() const
{
	return (WeaponType == EBotWeaponType::Shotgun) ? ShotgunPellets : 1;
}

// ---------------------------------------------------------------------------
// 5. Flee module (integrated into movement)
// ---------------------------------------------------------------------------
void UUrbanSimpleBotComponent::FleeFrom(const FVector& EnemyLocation)
{
	APawn* Pawn = GetBotController() ? GetBotController()->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const FVector Away = (Pawn->GetActorLocation() - EnemyLocation).GetSafeNormal2D();
	MoveInDirection(Away.IsNearlyZero() ? FVector::ForwardVector : Away);
}

// ---------------------------------------------------------------------------
// 6. Patrol module
// ---------------------------------------------------------------------------
void UUrbanSimpleBotComponent::Patrol(float DeltaTime)
{
	PatrolTimer -= DeltaTime;
	if (PatrolTimer <= 0.0f)
	{
		// Pick a new random horizontal direction.
		const float Angle = FMath::FRandRange(0.0f, 360.0f);
		PatrolDirection = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)),
		                          FMath::Sin(FMath::DegreesToRadians(Angle)),
		                          0.0f);
		PatrolTimer = PatrolChangeInterval;
	}

	// If we are stuck (barely moving) pick a new direction sooner.
	if (APawn* Pawn = GetBotController() ? GetBotController()->GetPawn() : nullptr)
	{
		const FVector Velocity2D = Pawn->GetVelocity();
		if (Velocity2D.Size2D() < 30.0f && PatrolTimer > 0.5f)
		{
			// Stuck (usually a wall): turn the current direction sideways by
			// 60-120 degrees instead of picking a fully random one, so the bot
			// slides along the obstacle instead of repeatedly ramming it.
			const float TurnAngle = FMath::FRandRange(60.0f, 120.0f)
				* (FMath::RandBool() ? 1.0f : -1.0f);
			PatrolDirection = PatrolDirection.RotateAngleAxis(TurnAngle, FVector::UpVector);
			PatrolTimer = FMath::Min(PatrolTimer, 0.5f);
		}
	}

	MoveInDirection(PatrolDirection);
	FireReleased();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool UUrbanSimpleBotComponent::HasWeapon() const
{
	// Every bot is assigned a weapon type at spawn (SetWeaponType), and firing
	// is a hit-scan ray rather than a physical equipment instance, so a bot
	// always counts as armed.
	return true;
}
