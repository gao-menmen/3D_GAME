#pragma once

#include "Components/ActorComponent.h"

#include "UrbanSimpleBotComponent.generated.h"

class AActor;
class AAIController;
class APawn;

/**
 * Bot weapon type. Each type has its own damage, fire rate, range and pellet
 * count, so a red/blue bot squad can mix weapons instead of everyone using
 * the same pistol.
 */
UENUM(BlueprintType)
enum class EBotWeaponType : uint8
{
	Pistol,
	Rifle,
	Shotgun
};

/** Immutable combat values resolved for the bot's selected weapon. */
struct FUrbanBotWeaponProfile
{
	float Damage = 0.0f;
	float FireInterval = 0.0f;
	float Range = 0.0f;
	int32 Pellets = 1;
	int32 MagazineSize = 0;
};

/**
 * Simple, self-contained bot AI. No behavior tree, no perception component -
 * every module is a plain function driven by TickComponent:
 *
 *  1. MoveInDirection    - move the pawn along a world direction.
 *  2. TurnAndShoot       - face a direction and fire the equipped weapon.
 *  3. FindVisibleEnemy   - scan a 120-degree cone in front of the bot for
 *                          enemies not blocked by walls; outputs a direction.
 *  4. DecideAndAct       - pick move vs shoot based on what weapon is held.
 *  5. FleeFrom (in move) - back away from a close enemy while moving.
 *  6. Patrol             - wander freely when no enemy is visible.
 *
 * Attached to the bot's AIController. Enemy = pawn whose team differs from
 * the bot's own team (via the Lyra team interface on player states).
 */
UCLASS()
class URBANCORE_API UUrbanSimpleBotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUrbanSimpleBotComponent(const FObjectInitializer& ObjectInitializer);

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~End of UActorComponent interface

	// 1. Movement module: steer the pawn along a world direction.
	//    Direction must be a unit vector; SpeedScale 0 stops.
	void MoveInDirection(const FVector& WorldDirection, float SpeedScale = 1.0f);

	// 2. Shooting module: face the direction and fire the equipped weapon.
	void TurnAndShoot(const FVector& TargetDirection);

	// 3. Vision module: find the nearest visible enemy inside a 120-degree
	//    cone ahead, not blocked by walls. Returns false if none.
	bool FindVisibleEnemy(FVector& OutDirection, AActor*& OutEnemy) const;

	// 5. Flee: move directly away from the given world location.
	void FleeFrom(const FVector& EnemyLocation);

	// Tunables.
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	float SightRange = 2400.0f;          // max distance the bot can see

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	float ConeHalfAngleDegrees = 60.0f;  // 120-degree total cone

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	float FleeDistance = 500.0f;         // enemies closer than this -> flee

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	float PatrolChangeInterval = 4.0f;   // seconds before a new patrol direction

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot")
	float RespawnDelay = 3.0f;           // seconds between the bot's death and its respawn

	// Per-weapon combat stats. Assigned at spawn by the spawner; see
	// EBotWeaponType. Shots are hit-scan sweeps along the bot's facing.
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	EBotWeaponType WeaponType = EBotWeaponType::Pistol;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float PistolDamage = 20.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float PistolInterval = 0.55f;
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float PistolRange = 3000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float RifleDamage = 10.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float RifleInterval = 0.30f;
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float RifleRange = 3500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float ShotgunDamage = 8.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float ShotgunInterval = 1.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	float ShotgunRange = 1500.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Urban|Bot|Weapon")
	int32 ShotgunPellets = 3;            // pellets fired per shotgun blast

	// Sets the bot's weapon type and its derived combat stats.
	void SetWeaponType(EBotWeaponType InType);

	// Returns the complete validated combat profile for the selected weapon.
	FUrbanBotWeaponProfile GetWeaponProfile() const;

	// Throws one grenade at the current locked target (limited stock).
	void ThrowGrenade();

	// Throws one smoke grenade at the current locked target (limited stock).
	void ThrowSmokeGrenade();

	// Plays the fire/reload animation montage for the current weapon on the
	// bot's mesh, so shots look like real shooting instead of a silent
	// hit-scan.
	void PlayWeaponMontage(const TCHAR* MontagePath, float Rate = 1.0f);

	// Replaces the pawn's invisible placeholder skeletal mesh (the character-
	// camera milestone hides the player's own body with an "Invis" mesh) with
	// the visible Mannequin mesh + base animation, so other players can see
	// the bot. Only called on bot pawns - the human player's own pawn keeps
	// its invisible mesh so the first-person camera stays clear. Public so the
	// spawner can apply it to freshly created bots as well.
	void EnsureVisibleMesh(APawn* BotPawn);

	// Equips the starter pistol directly on the bot's equipment manager (the
	// human player gets weapons through the weapon-selection UI; bots have no
	// such path, and without a weapon they close in on the player but never
	// actually fire). Public so the spawner can arm freshly created bots.
	void EquipStarterWeapon(APawn* BotPawn);

protected:
	// Combat stats for the current weapon type.
	float GetShotDamage() const;
	float GetShotInterval() const;
	float GetShotRange() const;
	int32 GetPellets() const;

	// Ammunition: the mag holds a limited number of shots, and the bot plays
	// a reload animation and pauses firing once empty, so the fight has a
	// visible shoot/reload rhythm.
	int32 GetMagazineSize() const;
	bool NeedsReload() const { return AmmoInMagazine <= 0; }
	void StartReload();

	// The fire/reload montage path for the current weapon type.
	const TCHAR* GetFireMontagePath() const;
	const TCHAR* GetReloadMontagePath() const;

	// Steers around walls: returns a direction with no obstacle in front of
	// the pawn (probing sideways up to 180 degrees), so bots glide along
	// walls and around corners instead of grinding into them. Not const: it
	// latches a side-bias so the steer direction doesn't flip every frame.
	FVector AdjustDirectionForObstacles(const APawn* Pawn, const FVector& DesiredDir);

	// 4. Decision module: choose move/shoot based on held weapon and distance.
	void DecideAndAct(float DeltaTime, const FVector& EnemyDirection, AActor* Enemy);

	// Respawn after the bot's pawn dies or is destroyed, so the arena keeps
	// its population. The controller (and its player state/team) survives the
	// pawn's death; only the pawn is re-created.
	void HandleRespawn(float DeltaTime);

	// Preferred engagement distance for the currently held weapon (units).
	float GetPreferredEngageDistance() const;

	// 6. Patrol module: wander when nothing is visible.
	void Patrol(float DeltaTime);

	bool IsEnemy(const AActor* Other) const;
	bool HasWeapon() const;
	void FirePressed();
	void FireReleased();

	// Toggles the Event.Movement.ADS gameplay tag on the bot's ability system.
	// ABP_Mannequin_Base reads this tag into its GameplayTag_IsADS variable and
	// switches into the FullBody_Aiming pose (gun raised to the shoulder), so
	// the bot visibly aims while shooting instead of firing from the jog pose.
	void SetAiming(bool bAiming);

	void FaceDirection(const FVector& WorldDirection);

	AAIController* GetBotController() const;

	UPROPERTY()
	TObjectPtr<AActor> LockedTarget;

	float FireTimer = 0.0f;
	float PatrolTimer = 0.0f;
	FVector PatrolDirection = FVector::ForwardVector;
	float RespawnTimer = 0.0f;
	int32 AmmoInMagazine = 12;
	float ReloadTimer = 0.0f;

	// Avoidance: which side the bot is currently steering around an obstacle
	// (-1 = left, +1 = right, 0 = none). Locked so the bot doesn't flip its
	// steer direction every frame and jitter against a wall.
	int32 AvoidSideBias = 0;

	// Combat strafe: which side the bot is currently strafing while shooting
	// (+1 = right, -1 = left) and when to switch to the other side.
	float StrafeTimer = 0.0f;
	int32 StrafeDir = 1;

	// Grenades remaining this life (thrown directly as projectiles).
	int32 GrenadeCount = 2;

	// Smoke grenades remaining this life.
	int32 SmokeGrenadeCount = 2;
};
