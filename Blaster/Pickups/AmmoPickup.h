
#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "AmmoPickup.generated.h"

/**
 *
 */
UCLASS()
class BLASTER_API AAmmoPickup : public APickup
{
	GENERATED_BODY()
public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
        EWeaponType WeaponType; // Use EWeaponType here

    UFUNCTION(BlueprintPure, Category = "Pickup")
        EWeaponType GetWeaponType() const
    {
        return WeaponType;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 AmmoAmount;

    int32 GetAmmoAmount() const
    {
        return AmmoAmount;
    }


protected:

    virtual void OnSphereOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult
    ) override;

    virtual void OnSphereEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex
    ) override;

};