#include "Combat/UrbanDamageModel.h"

FUrbanDamageResult UUrbanDamageModel::CalculateDamage(
    const float CurrentHealth,
    const float CurrentArmor,
    const FUrbanDamageProfile& DamageProfile,
    const FUrbanDamageRequest& DamageRequest)
{
    FUrbanDamageProfile SafeProfile = DamageProfile;
    SafeProfile.Sanitize();

    FUrbanDamageResult Result;
    Result.RemainingHealth = FMath::Max(CurrentHealth, 0.0f);
    Result.RemainingArmor = FMath::Max(CurrentArmor, 0.0f);

    const float ScaledDamage = FMath::Max(DamageRequest.RawDamage, 0.0f)
        * SafeProfile.GetRegionMultiplier(DamageRequest.HitRegion);

    float PreventedHealthDamage = 0.0f;
    const bool bArmorApplies = DamageRequest.bCanDamageArmor
        && DamageRequest.HitRegion == EUrbanHitRegion::Torso
        && Result.RemainingArmor > 0.0f;

    if (bArmorApplies)
    {
        const float DesiredPreventedDamage = ScaledDamage * SafeProfile.TorsoArmorAbsorption;
        const float DesiredArmorDamage = DesiredPreventedDamage * SafeProfile.ArmorDurabilityDamageScale;
        Result.AppliedArmorDamage = FMath::Min(DesiredArmorDamage, Result.RemainingArmor);
        PreventedHealthDamage = Result.AppliedArmorDamage / SafeProfile.ArmorDurabilityDamageScale;
        Result.RemainingArmor -= Result.AppliedArmorDamage;
    }

    const float UnclampedHealthDamage = FMath::Max(ScaledDamage - PreventedHealthDamage, 0.0f);
    Result.AppliedHealthDamage = FMath::Min(UnclampedHealthDamage, Result.RemainingHealth);
    Result.RemainingHealth -= Result.AppliedHealthDamage;
    Result.bKilled = Result.RemainingHealth <= KINDA_SMALL_NUMBER && ScaledDamage > 0.0f;
    return Result;
}
