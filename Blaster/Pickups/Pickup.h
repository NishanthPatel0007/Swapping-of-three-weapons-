// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "Pickup.generated.h"

class UCharacterOverlay;
class ABlasterHUD;
class UTexture2D;

UCLASS()
class BLASTER_API APickup : public AActor
{
	GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    APickup();

    UPROPERTY(EditAnywhere)
        class USoundCue* EquipSound;

    UPROPERTY(VisibleAnywhere)
        USphereComponent* SphereComponent;

    UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "UI")
        UTexture2D* PickupImage;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
        bool bIsVisible = true;


    UFUNCTION(BlueprintCallable, Category = "Pickup")
        void ShowPickupWidget(bool bShowImage);

    bool CheckSphereOverlap();

protected:

    void Tick(float DeltaTime);
    void BeginPlay();

    // Called when the sphere component overlaps something
    UFUNCTION()
        virtual void OnSphereOverlap(
            UPrimitiveComponent* OverlappedComponent,
            AActor* OtherActor,
            UPrimitiveComponent* OtherComp,
            int32 OtherBodyIndex,
            bool bFromSweep,
            const FHitResult& SweepResult
        );

    // Called when the sphere component ends overlapping something
    UFUNCTION()
        virtual void OnSphereEndOverlap(
            UPrimitiveComponent* OverlappedComponent,
            AActor* OtherActor,
            UPrimitiveComponent* OtherComp,
            int32 OtherBodyIndex
        );




private:

    UPROPERTY()
        class UCharacterOverlay* MyCharacterOverlay;

    UPROPERTY()
        class ABlasterHUD* MyBlasterHUD;

    UPROPERTY(VisibleAnywhere, Category = "pickup Properties")
        USkeletalMeshComponent* MyPickups;

    UPROPERTY(VisibleAnywhere, Category = "pickup Properties")
        class USphereComponent* AreaSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
        class UUserWidget* PickupWidget;


};
