// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Image.h"
#include "Blaster/HUD/CharacterOverlay.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/Character/BlasterCharacter.h"


// Constructor
APickup::APickup()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    MyPickups = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    SetRootComponent(MyPickups);

    MyPickups->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    MyPickups->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
    MyPickups->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MyPickups->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
    AreaSphere->SetupAttachment(RootComponent); // Attach to the root component
    AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);


}

// Called when the game starts or when spawned
void APickup::BeginPlay()
{
    Super::BeginPlay();

    AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
    AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &APickup::OnSphereOverlap);
    AreaSphere->OnComponentEndOverlap.AddDynamic(this, &APickup::OnSphereEndOverlap);

}

// Called every frame
void APickup::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Check if the sphere is currently overlapping with any actors
    bool bIsOverlapping = CheckSphereOverlap(); // Implement your overlap check logic
    ShowPickupWidget(bIsOverlapping);    // Call ShowPickupWidget based on the overlap status

}

bool APickup::CheckSphereOverlap()
{
    if (SphereComponent)
    {
        TArray<AActor*> OverlappingActors;
        SphereComponent->GetOverlappingActors(OverlappingActors, ABlasterCharacter::StaticClass());

        // Check if there are overlapping actors
        return OverlappingActors.Num() > 0;
    }

    return false;
}

void APickup::ShowPickupWidget(bool bShowImage)
{
    if (PickupImage)
    {
        if (PickupWidget)
        {
            UImage* ImageWidget = Cast<UImage>(PickupWidget->GetWidgetFromName(TEXT("YourImageWidgetName")));

            if (ImageWidget)
            {
                ImageWidget->SetVisibility(bShowImage ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
            }
        }
    }
}

void APickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ABlasterCharacter* Character = Cast<ABlasterCharacter>(OtherActor);
    if (Character)
    {
        ShowPickupWidget(true);
    }
}

// Called when the area sphere ends overlapping something
void APickup::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    ABlasterCharacter* Character = Cast<ABlasterCharacter>(OtherActor);
    if (Character)
    {
        ShowPickupWidget(false);
    }
}
