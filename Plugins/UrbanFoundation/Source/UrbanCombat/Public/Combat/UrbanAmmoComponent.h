#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/UrbanCombatTypes.h"
#include "UrbanAmmoComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUrbanAmmoStateChanged);

UCLASS(ClassGroup = (UrbanSpear), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class URBANCOMBAT_API UUrbanAmmoComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUrbanAmmoComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Urban Spear|Combat|Ammo")
    const FUrbanAmmoState& GetAmmoState() const { return AmmoState; }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Urban Spear|Combat|Ammo")
    bool ConfigureAuthoritative(const FUrbanWeaponTuning& WeaponTuning);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Urban Spear|Combat|Ammo")
    bool TryConsumeRoundAuthoritative();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Urban Spear|Combat|Ammo")
    bool BeginReloadAuthoritative();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Urban Spear|Combat|Ammo")
    int32 CompleteReloadAuthoritative();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Urban Spear|Combat|Ammo")
    bool CancelReloadAuthoritative();

    UPROPERTY(BlueprintAssignable, Category = "Urban Spear|Combat|Ammo")
    FUrbanAmmoStateChanged OnAmmoStateChanged;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_AmmoState, VisibleAnywhere, BlueprintReadOnly, Category = "Urban Spear|Combat|Ammo")
    FUrbanAmmoState AmmoState;

    UFUNCTION()
    void OnRep_AmmoState();

private:
    bool HasServerAuthority() const;
    void BroadcastAmmoStateChanged();
};
