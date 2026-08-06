#pragma once

#include "CoreMinimal.h"
#include "UrbanCombatTypes.generated.h"

UENUM(BlueprintType)
enum class EUrbanWeaponArchetype : uint8
{
    AssaultRifle,
    SubmachineGun,
    TacticalPistol,
};

UENUM(BlueprintType)
enum class EUrbanFireMode : uint8
{
    SemiAutomatic,
    Automatic,
};

UENUM(BlueprintType)
enum class EUrbanHitRegion : uint8
{
    Head,
    Torso,
    Limb,
};

UENUM(BlueprintType)
enum class EUrbanTacticalItemType : uint8
{
    FragGrenade,
    SmokeGrenade,
    Medkit,
    ReconDrone,
};

USTRUCT(BlueprintType)
struct URBANCOMBAT_API FUrbanWeaponTuning
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    EUrbanWeaponArchetype Archetype = EUrbanWeaponArchetype::AssaultRifle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    FName WeaponId = TEXT("US_AR4");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    int32 MagazineCapacity = 30;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    int32 StartingReserveAmmo = 120;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float RoundsPerMinute = 700.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float BaseDamage = 32.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float MaximumRangeMeters = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float HipFireSpreadDegrees = 2.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float AimSpreadDegrees = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float VerticalRecoil = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float HorizontalRecoil = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    bool bSupportsSemiAutomatic = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    bool bSupportsAutomatic = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    EUrbanFireMode PreferredFireMode = EUrbanFireMode::Automatic;

    void Sanitize();
    bool SupportsFireMode(EUrbanFireMode FireMode) const;
    float GetShotIntervalSeconds() const;

    static FUrbanWeaponTuning MakeDefaults(EUrbanWeaponArchetype InArchetype);
};

USTRUCT(BlueprintType)
struct URBANCOMBAT_API FUrbanAmmoState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
    int32 MagazineCapacity = 30;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
    int32 AmmoInMagazine = 30;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
    int32 ReserveAmmo = 120;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
    bool bReloading = false;

    void Initialize(int32 InMagazineCapacity, int32 InAmmoInMagazine, int32 InReserveAmmo);
    void Sanitize();
    bool CanFire() const;
    bool TryConsumeRound();
    bool CanReload() const;
    bool BeginReload();
    int32 CompleteReload();
    void CancelReload();
};

USTRUCT(BlueprintType)
struct URBANCOMBAT_API FUrbanDamageProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float HeadMultiplier = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float TorsoMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float LimbMultiplier = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float TorsoArmorAbsorption = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
    float ArmorDurabilityDamageScale = 1.0f;

    void Sanitize();
    float GetRegionMultiplier(EUrbanHitRegion HitRegion) const;
};

USTRUCT(BlueprintType)
struct URBANCOMBAT_API FUrbanDamageRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float RawDamage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    EUrbanHitRegion HitRegion = EUrbanHitRegion::Torso;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    bool bCanDamageArmor = true;
};

USTRUCT(BlueprintType)
struct URBANCOMBAT_API FUrbanDamageResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    float AppliedHealthDamage = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    float AppliedArmorDamage = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    float RemainingHealth = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    float RemainingArmor = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    bool bKilled = false;
};

USTRUCT(BlueprintType)
struct URBANCOMBAT_API FUrbanTacticalItemTuning
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Item")
    EUrbanTacticalItemType ItemType = EUrbanTacticalItemType::FragGrenade;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Item")
    FName ItemId = TEXT("US_FRAG");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Item")
    int32 MaximumCharges = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Item")
    float CooldownSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Item")
    float EffectDurationSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tactical Item")
    float MaximumUseRangeMeters = 25.0f;

    void Sanitize();
    static FUrbanTacticalItemTuning MakeDefaults(EUrbanTacticalItemType InItemType);
};
