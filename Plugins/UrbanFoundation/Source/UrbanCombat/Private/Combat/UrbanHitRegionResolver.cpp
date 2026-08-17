#include "Combat/UrbanHitRegionResolver.h"

EUrbanHitRegion UUrbanHitRegionResolver::ResolveBoneName(const FName BoneName)
{
    if (BoneName.IsNone())
    {
        return EUrbanHitRegion::Torso;
    }

    const FString Bone = BoneName.ToString().ToLower();
    if (Bone.Contains(TEXT("head")))
    {
        return EUrbanHitRegion::Head;
    }

    static const TCHAR* TorsoTokens[] =
    {
        TEXT("pelvis"), TEXT("spine"), TEXT("chest"), TEXT("clavicle"), TEXT("neck")
    };
    for (const TCHAR* Token : TorsoTokens)
    {
        if (Bone.Contains(Token))
        {
            return EUrbanHitRegion::Torso;
        }
    }

    return EUrbanHitRegion::Limb;
}
