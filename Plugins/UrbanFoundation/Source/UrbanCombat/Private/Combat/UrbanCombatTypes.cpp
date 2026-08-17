#include "Combat/UrbanCombatTypes.h"

void FUrbanWeaponTuning::Sanitize()
{
    if (WeaponId.IsNone())
    {
        WeaponId = TEXT("US_WEAPON");
    }

    MagazineCapacity = FMath::Clamp(MagazineCapacity, 1, 500);
    StartingReserveAmmo = FMath::Clamp(StartingReserveAmmo, 0, 5000);
    RoundsPerMinute = FMath::Clamp(RoundsPerMinute, 60.0f, 1500.0f);
    BaseDamage = FMath::Clamp(BaseDamage, 1.0f, 500.0f);
    MaximumRangeMeters = FMath::Clamp(MaximumRangeMeters, 1.0f, 2000.0f);
    HipFireSpreadDegrees = FMath::Clamp(HipFireSpreadDegrees, 0.0f, 30.0f);
    AimSpreadDegrees = FMath::Clamp(AimSpreadDegrees, 0.0f, HipFireSpreadDegrees);
    VerticalRecoil = FMath::Clamp(VerticalRecoil, 0.0f, 20.0f);
    HorizontalRecoil = FMath::Clamp(HorizontalRecoil, 0.0f, 20.0f);

    if (!bSupportsSemiAutomatic && !bSupportsAutomatic)
    {
        bSupportsSemiAutomatic = true;
    }

    if (!SupportsFireMode(PreferredFireMode))
    {
        PreferredFireMode = bSupportsAutomatic ? EUrbanFireMode::Automatic : EUrbanFireMode::SemiAutomatic;
    }
}

bool FUrbanWeaponTuning::SupportsFireMode(const EUrbanFireMode FireMode) const
{
    return FireMode == EUrbanFireMode::Automatic ? bSupportsAutomatic : bSupportsSemiAutomatic;
}

float FUrbanWeaponTuning::GetShotIntervalSeconds() const
{
    return 60.0f / FMath::Max(RoundsPerMinute, 1.0f);
}

FUrbanWeaponTuning FUrbanWeaponTuning::MakeDefaults(const EUrbanWeaponArchetype InArchetype)
{
    FUrbanWeaponTuning Tuning;
    Tuning.Archetype = InArchetype;

    switch (InArchetype)
    {
    case EUrbanWeaponArchetype::AssaultRifle:
        Tuning.WeaponId = TEXT("US_AR4");
        Tuning.MagazineCapacity = 30;
        Tuning.StartingReserveAmmo = 120;
        Tuning.RoundsPerMinute = 700.0f;
        Tuning.BaseDamage = 32.0f;
        Tuning.MaximumRangeMeters = 180.0f;
        Tuning.HipFireSpreadDegrees = 2.2f;
        Tuning.AimSpreadDegrees = 0.35f;
        Tuning.VerticalRecoil = 1.0f;
        Tuning.HorizontalRecoil = 0.35f;
        Tuning.bSupportsSemiAutomatic = true;
        Tuning.bSupportsAutomatic = true;
        Tuning.PreferredFireMode = EUrbanFireMode::Automatic;
        break;

    case EUrbanWeaponArchetype::SubmachineGun:
        Tuning.WeaponId = TEXT("US_SMG9");
        Tuning.MagazineCapacity = 35;
        Tuning.StartingReserveAmmo = 140;
        Tuning.RoundsPerMinute = 900.0f;
        Tuning.BaseDamage = 24.0f;
        Tuning.MaximumRangeMeters = 90.0f;
        Tuning.HipFireSpreadDegrees = 1.8f;
        Tuning.AimSpreadDegrees = 0.45f;
        Tuning.VerticalRecoil = 0.75f;
        Tuning.HorizontalRecoil = 0.5f;
        Tuning.bSupportsSemiAutomatic = false;
        Tuning.bSupportsAutomatic = true;
        Tuning.PreferredFireMode = EUrbanFireMode::Automatic;
        break;

    case EUrbanWeaponArchetype::TacticalPistol:
        Tuning.WeaponId = TEXT("US_P12");
        Tuning.MagazineCapacity = 15;
        Tuning.StartingReserveAmmo = 60;
        Tuning.RoundsPerMinute = 420.0f;
        Tuning.BaseDamage = 28.0f;
        Tuning.MaximumRangeMeters = 60.0f;
        Tuning.HipFireSpreadDegrees = 2.0f;
        Tuning.AimSpreadDegrees = 0.3f;
        Tuning.VerticalRecoil = 1.2f;
        Tuning.HorizontalRecoil = 0.25f;
        Tuning.bSupportsSemiAutomatic = true;
        Tuning.bSupportsAutomatic = false;
        Tuning.PreferredFireMode = EUrbanFireMode::SemiAutomatic;
        break;
    }

    Tuning.Sanitize();
    return Tuning;
}

void FUrbanAmmoState::Initialize(const int32 InMagazineCapacity, const int32 InAmmoInMagazine, const int32 InReserveAmmo)
{
    MagazineCapacity = InMagazineCapacity;
    AmmoInMagazine = InAmmoInMagazine;
    ReserveAmmo = InReserveAmmo;
    bReloading = false;
    Sanitize();
}

void FUrbanAmmoState::Sanitize()
{
    MagazineCapacity = FMath::Clamp(MagazineCapacity, 1, 500);
    AmmoInMagazine = FMath::Clamp(AmmoInMagazine, 0, MagazineCapacity);
    ReserveAmmo = FMath::Clamp(ReserveAmmo, 0, 5000);
    if (AmmoInMagazine >= MagazineCapacity || ReserveAmmo <= 0)
    {
        bReloading = false;
    }
}

bool FUrbanAmmoState::CanFire() const
{
    return !bReloading && AmmoInMagazine > 0;
}

bool FUrbanAmmoState::TryConsumeRound()
{
    if (!CanFire())
    {
        return false;
    }

    --AmmoInMagazine;
    return true;
}

bool FUrbanAmmoState::CanReload() const
{
    return !bReloading && AmmoInMagazine < MagazineCapacity && ReserveAmmo > 0;
}

bool FUrbanAmmoState::BeginReload()
{
    if (!CanReload())
    {
        return false;
    }

    bReloading = true;
    return true;
}

int32 FUrbanAmmoState::CompleteReload()
{
    if (!bReloading)
    {
        return 0;
    }

    const int32 TransferredRounds = FMath::Min(MagazineCapacity - AmmoInMagazine, ReserveAmmo);
    AmmoInMagazine += TransferredRounds;
    ReserveAmmo -= TransferredRounds;
    bReloading = false;
    return TransferredRounds;
}

void FUrbanAmmoState::CancelReload()
{
    bReloading = false;
}

void FUrbanDamageProfile::Sanitize()
{
    HeadMultiplier = FMath::Clamp(HeadMultiplier, 0.0f, 10.0f);
    TorsoMultiplier = FMath::Clamp(TorsoMultiplier, 0.0f, 10.0f);
    LimbMultiplier = FMath::Clamp(LimbMultiplier, 0.0f, 10.0f);
    TorsoArmorAbsorption = FMath::Clamp(TorsoArmorAbsorption, 0.0f, 1.0f);
    HeadArmorAbsorption = FMath::Clamp(HeadArmorAbsorption, 0.0f, 1.0f);
    ArmorDurabilityDamageScale = FMath::Clamp(ArmorDurabilityDamageScale, 0.01f, 10.0f);
}

float FUrbanDamageProfile::GetRegionMultiplier(const EUrbanHitRegion HitRegion) const
{
    switch (HitRegion)
    {
    case EUrbanHitRegion::Head:
        return HeadMultiplier;
    case EUrbanHitRegion::Limb:
        return LimbMultiplier;
    case EUrbanHitRegion::Torso:
    default:
        return TorsoMultiplier;
    }
}

void FUrbanTacticalItemTuning::Sanitize()
{
    if (ItemId.IsNone())
    {
        ItemId = TEXT("US_ITEM");
    }
    MaximumCharges = FMath::Clamp(MaximumCharges, 1, 20);
    CooldownSeconds = FMath::Clamp(CooldownSeconds, 0.0f, 300.0f);
    EffectDurationSeconds = FMath::Clamp(EffectDurationSeconds, 0.0f, 300.0f);
    MaximumUseRangeMeters = FMath::Clamp(MaximumUseRangeMeters, 0.0f, 1000.0f);
}

FUrbanTacticalItemTuning FUrbanTacticalItemTuning::MakeDefaults(const EUrbanTacticalItemType InItemType)
{
    FUrbanTacticalItemTuning Tuning;
    Tuning.ItemType = InItemType;

    switch (InItemType)
    {
    case EUrbanTacticalItemType::FragGrenade:
        Tuning.ItemId = TEXT("US_FRAG");
        Tuning.MaximumCharges = 2;
        Tuning.CooldownSeconds = 1.0f;
        Tuning.EffectDurationSeconds = 0.0f;
        Tuning.MaximumUseRangeMeters = 35.0f;
        break;
    case EUrbanTacticalItemType::SmokeGrenade:
        Tuning.ItemId = TEXT("US_SMOKE");
        Tuning.MaximumCharges = 2;
        Tuning.CooldownSeconds = 1.0f;
        Tuning.EffectDurationSeconds = 18.0f;
        Tuning.MaximumUseRangeMeters = 35.0f;
        break;
    case EUrbanTacticalItemType::Medkit:
        Tuning.ItemId = TEXT("US_MEDKIT");
        Tuning.MaximumCharges = 2;
        Tuning.CooldownSeconds = 6.0f;
        Tuning.EffectDurationSeconds = 3.0f;
        Tuning.MaximumUseRangeMeters = 2.0f;
        break;
    case EUrbanTacticalItemType::ReconDrone:
        Tuning.ItemId = TEXT("US_RECON_DRONE");
        Tuning.MaximumCharges = 1;
        Tuning.CooldownSeconds = 45.0f;
        Tuning.EffectDurationSeconds = 20.0f;
        Tuning.MaximumUseRangeMeters = 80.0f;
        break;
    }

    Tuning.Sanitize();
    return Tuning;
}
