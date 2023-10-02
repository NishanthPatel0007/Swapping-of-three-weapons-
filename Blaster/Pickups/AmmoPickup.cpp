// Fill out your copyright notice in the Description page of Project Settings.


#include "AmmoPickup.h"
#include "Components/SceneComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/SphereComponent.h" 
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/Character/BlasterCharacter.h"




void AAmmoPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    Super::OnSphereOverlap(OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
    if (BlasterCharacter)
    {
        BlasterCharacter->SetOverlappingPickup(this);
    }
}

void AAmmoPickup::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    Super::OnSphereEndOverlap(OverlappedComp, OtherActor, OtherComp, OtherBodyIndex);

    ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
    if (BlasterCharacter)
    {
        BlasterCharacter->SetOverlappingPickup(nullptr);
    }
}
