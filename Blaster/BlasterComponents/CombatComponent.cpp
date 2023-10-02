// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Sound/SoundCue.h"
#include "Blaster/Character/BlasterAnimInstance.h"
#include "Blaster/Weapon/Projectile.h"
#include "Blaster/Weapon/Shotgun.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	InitializeWeaponMaps();

	BaseWalkSpeed = 600.f;
	AimWalkSpeed = 450.f;
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, BackpackWeapon1);
	DOREPLIFETIME(UCombatComponent, BackpackWeapon2);
	DOREPLIFETIME(UCombatComponent, bAiming);
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly);
	DOREPLIFETIME(UCombatComponent, CombatState);
	DOREPLIFETIME(UCombatComponent, Grenades);
}

void UCombatComponent::ShotgunShellReload()
{
	if (Character && Character->HasAuthority())
	{
		UpdateShotgunAmmoValues();
	}

}
bool UCombatComponent::PickupAmmo(EWeaponType My_WeaponType, int32 My_AmmoAmount)
{
	// Check if the CarriedAmmoMap contains the specified weapon type
	if (CarriedAmmoMap.Contains(My_WeaponType))
	{
		// Add the AmmoAmount to the existing carried ammo for the specified weapon type,
		// ensuring that the value is clamped between 0 and MaxCarriedAmmo
		CarriedAmmoMap[My_WeaponType] = FMath::Clamp(CarriedAmmoMap[My_WeaponType] + My_AmmoAmount, 0, MaxCarriedAmmo);

		// Update the carried ammo value in the HUD
		UpdateCarriedAmmo();

		// Set bLocalPickupSuccessful to true to indicate successful pickup
		bLocalPickupSuccessful = true;
	}

	// Check if the currently equipped weapon is empty and has the same weapon type as the one being picked up
	if (EquippedWeapon && EquippedWeapon->IsEmpty() && EquippedWeapon->GetWeaponType() == My_WeaponType)
	{
		// Reload the weapon, which involves adding ammo to it
		Reload();
	}

	// Return the value of bLocalPickupSuccessful to indicate whether ammo was successfully picked up
	return bLocalPickupSuccessful;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Character)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

		if (Character->GetFollowCamera())
		{
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}
		if (Character->HasAuthority())
		{
			InitializeCarriedAmmo();
		}
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character && Character->IsLocallyControlled())
	{
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;

		SetHUDCrosshairs(DeltaTime);
		InterpFOV(DeltaTime);
	}
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{
	bFireButtonPressed = bPressed;
	if (bFireButtonPressed)
	{
		Fire();
	}
}

AWeapon* UCombatComponent::IdentifyHoldingWeapon() const
{
	if (EquippedWeapon && EquippedWeapon->GetWeaponState() == EWeaponState::EWS_Equipped)
	{
		return EquippedWeapon;
	}
	else if (BackpackWeapon1 && BackpackWeapon1->GetWeaponState() == EWeaponState::EWS_Equipped)
	{
		return BackpackWeapon1;
	}
	else if (BackpackWeapon2 && BackpackWeapon2->GetWeaponState() == EWeaponState::EWS_Equipped)
	{
		return BackpackWeapon2;
	}
	return nullptr;
}

void UCombatComponent::Fire()
{
	if (CanFire())
	{
		bCanFire = false;
		if (EquippedWeapon)
		{
			CrosshairShootingFactor = .65f;

			switch (EquippedWeapon->FireType)
			{
			case EFireType::EFT_Projectile:
				FireProjectileWeapon();
				break;
			case EFireType::EFT_HitScan:
				FireHitScanWeapon();
				break;
			case EFireType::EFT_Shotgun:
				FireShotgun();
				break;
			}
		}
		StartFireTimer();
	}
}
void UCombatComponent::FireProjectileWeapon()
{
	if (EquippedWeapon && Character)
	{
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		if (!Character->HasAuthority()) LocalFire(HitTarget);
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireHitScanWeapon()
{
	if (EquippedWeapon && Character)
	{
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		if (!Character->HasAuthority()) LocalFire(HitTarget);
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireShotgun()
{
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	if (Shotgun && Character)
	{
		TArray<FVector_NetQuantize> HitTargets;
		Shotgun->ShotgunTraceEndWithScatter(HitTarget, HitTargets);
		if (!Character->HasAuthority()) ShotgunLocalFire(HitTargets);
		ServerShotgunFire(HitTargets, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::StartFireTimer()
{
	if (EquippedWeapon == nullptr || Character == nullptr) return;
	Character->GetWorldTimerManager().SetTimer(
		FireTimer,
		this,
		&UCombatComponent::FireTimerFinished,
		EquippedWeapon->FireDelay
	);
}

void UCombatComponent::FireTimerFinished()
{
	if (EquippedWeapon == nullptr) return;
	bCanFire = true;
	if (bFireButtonPressed && EquippedWeapon->bAutomatic)
	{
		Fire();
	}
	ReloadEmptyWeapon();
}

void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget, float FireDelay)
{
	MulticastFire(TraceHitTarget);
}

bool UCombatComponent::ServerFire_Validate(const FVector_NetQuantize& TraceHitTarget, float FireDelay)
{
	if (EquippedWeapon)
	{
		bool bNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
		return bNearlyEqual;
	}
	return true;
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	LocalFire(TraceHitTarget);
}

void UCombatComponent::ServerShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay)
{
	MulticastShotgunFire(TraceHitTargets);
}

bool UCombatComponent::ServerShotgunFire_Validate(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay)
{
	if (EquippedWeapon)
	{
		bool bNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
		return bNearlyEqual;
	}
	return true;
}

void UCombatComponent::MulticastShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets)
{
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	ShotgunLocalFire(TraceHitTargets);
}

void UCombatComponent::LocalFire(const FVector_NetQuantize& TraceHitTarget)
{
	if (EquippedWeapon == nullptr) return;
	if (Character && CombatState == ECombatState::ECS_Unoccupied)
	{
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::ShotgunLocalFire(const TArray<FVector_NetQuantize>& TraceHitTargets)
{
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	if (Shotgun == nullptr || Character == nullptr) return;
	if (CombatState == ECombatState::ECS_Reloading || CombatState == ECombatState::ECS_Unoccupied)
	{
		Character->PlayFireMontage(bAiming);
		Shotgun->FireShotgun(TraceHitTargets);
		CombatState = ECombatState::ECS_Unoccupied;
	}
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip)
{
    if (Character == nullptr || WeaponToEquip == nullptr)
    {
        return;
    }

    if (CombatState != ECombatState::ECS_Unoccupied)
    {
        return;
    }

    const auto FirstEmptyIndex = GetFirstFreeSlot();

    // If std::nullopt means all slots are occupied
    // we are dropping equipped weapon and picking a new one to hands
    if (!FirstEmptyIndex.has_value())
    {
        uint32 EquippedWeaponSlot = BackpackWeapon1->GetSlotIndex();
        if (EquippedWeaponSlot != 0)
        {
            WeaponToEquip->SetSlotIndex(BackpackWeapon1->GetSlotIndex());
            EquipWeaponToHands(WeaponToEquip);
            WeaponSlotsMap[WeaponToEquip->GetSlotIndex()] = true;
        }
    }
    else
    {
        WeaponToEquip->SetSlotIndex(FirstEmptyIndex.value());
        WeaponSlotsMap[FirstEmptyIndex.value()] = true;

        if (BackpackWeapon1 == nullptr)
        {
            EquipWeaponToHands(WeaponToEquip);
        }
        else if (BackpackWeapon2 == nullptr)
        {
            EquipWeaponToBackpack(WeaponToEquip, 1);
        }
		else if (BackpackWeapon3 == nullptr)
		{
			EquipWeaponToBackpack(WeaponToEquip, 2);
		}

    }

    Character->GetCharacterMovement()->bOrientRotationToMovement = false;
    Character->bUseControllerRotationYaw = true;
}
void UCombatComponent::EquipWeaponToBackpack(AWeapon* WeaponToEquip, uint32 SlotNumber)
{
	if (WeaponToEquip == nullptr)
	{
		return;
	}

	FName SocketName;

	switch (SlotNumber)
	{
	case 1:
		BackpackWeapon2 = WeaponToEquip;
		BackpackWeapon2->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
		BackpackWeapon2->SetOwner(Character);
		SocketName = FName("BackpackSocket2");
		break;
	case 2:
		BackpackWeapon3 = WeaponToEquip;
		BackpackWeapon3->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
		BackpackWeapon3->SetOwner(Character);
		SocketName = FName("BackpackSocket3");
		break;

	default:
		SocketName = FName("");
		break;
	}

	if (SocketName != FName(""))
	{
		WeaponToEquip->SetHUDWeaponSlot();
		AttachActorToBackpack(WeaponToEquip, SocketName);
		PlayEquipWeaponSound(WeaponToEquip);
	}
}

void UCombatComponent::SwapWeapons(uint32 SlotClicked)
{
	AWeapon* TempWeapon = nullptr;

	
	if (BackpackWeapon1 && BackpackWeapon1->GetSlotIndex() == SlotClicked)
	{
		TempWeapon = EquippedWeapon; 
		EquippedWeapon = BackpackWeapon1;

		if (BackpackWeapon1)
		{
			BackpackWeapon1->SetWeaponState(EWeaponState::EWS_Equipped);
			AttachActorToRightHand(BackpackWeapon1);
			BackpackWeapon1->SetHUDAmmo(); // Will handle it on server
			UpdateCarriedAmmo();
			PlayEquipWeaponSound(BackpackWeapon1);
		}

		// Set the weapon state of TempWeapon (previously EquippedWeapon) to "EWS_EquippedOnBackpack"
		if (TempWeapon)
		{
			TempWeapon->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(TempWeapon, "BackpackSocket1");
		}

		if (BackpackWeapon2)
		{
			BackpackWeapon2->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(BackpackWeapon2, "BackpackSocket2");
		}
		if (BackpackWeapon3)
		{
			BackpackWeapon3->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(BackpackWeapon3, "BackpackSocket3");
		}

	}
	else if (BackpackWeapon2 && BackpackWeapon2->GetSlotIndex() == SlotClicked)
	{
		TempWeapon = EquippedWeapon; 
		EquippedWeapon = BackpackWeapon2; 

		if (BackpackWeapon2)
		{
			BackpackWeapon2->SetWeaponState(EWeaponState::EWS_Equipped);
			AttachActorToRightHand(BackpackWeapon2);
			BackpackWeapon2->SetHUDAmmo(); // Will handle it on server
			UpdateCarriedAmmo();
			PlayEquipWeaponSound(BackpackWeapon2);
		}
		if (TempWeapon)
		{
			TempWeapon->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(TempWeapon, "BackpackSocket2");
		}
		if (BackpackWeapon1)
		{
			BackpackWeapon1->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(BackpackWeapon1, "BackpackSocket1");
		}
		if (BackpackWeapon3)
		{
			BackpackWeapon3->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(BackpackWeapon3, "BackpackSocket3");
		}

	}
	else if (BackpackWeapon3 && BackpackWeapon3->GetSlotIndex() == SlotClicked)
	{
		TempWeapon = EquippedWeapon; 
		EquippedWeapon = BackpackWeapon3;

		if (BackpackWeapon3)
		{
			BackpackWeapon3->SetWeaponState(EWeaponState::EWS_Equipped);
			AttachActorToRightHand(BackpackWeapon3);
			BackpackWeapon3->SetHUDAmmo(); // Will handle it on server
			UpdateCarriedAmmo();
			PlayEquipWeaponSound(BackpackWeapon3);
		}
		if (TempWeapon)
		{
			TempWeapon->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(TempWeapon, "BackpackSocket3");
		}
		if (BackpackWeapon1)
		{
			BackpackWeapon1->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(BackpackWeapon1, "BackpackSocket1");
		}
		if (BackpackWeapon2)
		{
			BackpackWeapon2->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(BackpackWeapon2, "BackpackSocket2");
		}
	}
}

void UCombatComponent::EquipWeaponToHands(AWeapon* WeaponToEquip)
{
	if (WeaponToEquip == nullptr)
	{
		return;
	}
	DropEquippedWeapon();

	BackpackWeapon1 = WeaponToEquip;
	BackpackWeapon1->SetWeaponState(EWeaponState::EWS_Equipped);

	// This is automatically replicated
	AttachActorToRightHand(BackpackWeapon1);

	//Owner is automatically replicated
	// We could use OnRep_Owner notifier - override it
	BackpackWeapon1->SetOwner(Character);
	BackpackWeapon1->SetHUDAmmo(); //To handle it on server
	BackpackWeapon1->SetHUDWeaponSlot(); //To handle it on server

	UpdateCarriedAmmo();
	PlayEquipWeaponSound(WeaponToEquip);
	ReloadEmptyWeapon();
}

void UCombatComponent::DropEquippedWeapon()
{
	if (BackpackWeapon1)
	{
		int32 IndexForDroppedWeapon = BackpackWeapon1->GetSlotIndex();
		if (IndexForDroppedWeapon >= MinWeaponSlotIndex && IndexForDroppedWeapon <= MaxWeaponSlotIndex)
		{
			WeaponSlotsMap[BackpackWeapon1->GetSlotIndex()] = false;
			BackpackWeapon1->Dropped();
			BackpackWeapon1 = EmptyWeapon;
		}
	}
}

std::optional<int32> UCombatComponent::GetFirstFreeSlot()
{
	for (auto& Elem : WeaponSlotsMap)
	{
		if (Elem.Value == false)
		{
			return Elem.Key;
		}
	}
	return std::nullopt;
}

void UCombatComponent::InitializeWeaponMaps()
{
	WeaponSlotsMap.Add(1, false);
	WeaponSlotsMap.Add(2, false);
	WeaponSlotsMap.Add(3, false);
}


void UCombatComponent::Unequipped()
{
	if (EquippedWeapon)
	{
		if (BackpackWeapon1 == nullptr)
		{
			EquippedWeapon->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(EquippedWeapon, "BackpackSocket2");
			BackpackWeapon1 = EquippedWeapon;
			EquippedWeapon = nullptr;
		}
		else if (BackpackWeapon2 == nullptr)
		{
			EquippedWeapon->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(EquippedWeapon, "BackpackSocket3");
			BackpackWeapon2 = EquippedWeapon;
			EquippedWeapon = nullptr;
		}
		else
		{
			// Attach EquippedWeapon to BackpackSocket1
			EquippedWeapon->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			AttachActorToBackpack(EquippedWeapon, "BackpackSocket1");
			BackpackWeapon2 = EquippedWeapon;
			EquippedWeapon = nullptr;
		}
		// Handle any other cases if needed
	}
	// Handle any other cases for BackpackWeapon1 and BackpackWeapon2 if needed
}

void UCombatComponent::OnRep_Aiming()
{
	if (Character && Character->IsLocallyControlled())
	{
		bAiming = bAimButtonPressed;
	}
}


bool UCombatComponent::AttachActorToRightHand(AActor* ActorToAttach)
{
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr)
	{
		return false; // Indicate that attachment was unsuccessful
	}

	// This is automatically replicated
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket)
	{
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
		return true; // Indicate that attachment was successful
	}

	return false; // Indicate that attachment was unsuccessful
}


void UCombatComponent::AttachActorToLeftHand(AActor* ActorToAttach)
{
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr || EquippedWeapon == nullptr)
	{
		return;
	}
	bool bUsePistolSocket =
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol ||
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SubmachineGun;

	FName SocketName = bUsePistolSocket ? FName("PistolSocket") : FName("LeftHandSocket");

	// This is automatically replicated
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(SocketName);
	if (HandSocket)
	{
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::AttachActorToBackpack(AActor* ActorToAttach, const FName& SocketName)
{
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr)
	{
		return;
	}

	// This is automatically replicated
	const USkeletalMeshSocket* BackpackSocket = Character->GetMesh()->GetSocketByName(SocketName);
	if (BackpackSocket)
	{
		BackpackSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::AttachActorToBackpack2(AActor* ActorToAttach, const FName& SocketName)
{
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr)
	{
		return;
	}

	// This is automatically replicated
	const USkeletalMeshSocket* BackpackSocket3 = Character->GetMesh()->GetSocketByName(SocketName);
	if (BackpackSocket3)
	{
		BackpackSocket3->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::UpdateCarriedAmmo()
{
	if (EquippedWeapon == nullptr) return;
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}

	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
}

void UCombatComponent::PlayEquipWeaponSound(AWeapon* WeaponToEquip)
{
	if (Character && WeaponToEquip && WeaponToEquip->EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			WeaponToEquip->EquipSound,
			Character->GetActorLocation()
		);
	}
}

void UCombatComponent::ReloadEmptyWeapon()
{
	if (EquippedWeapon && EquippedWeapon->IsEmpty())
	{
		Reload();
	}
}

void UCombatComponent::Reload()
{
	if (CarriedAmmo > 0 && CombatState == ECombatState::ECS_Unoccupied && EquippedWeapon && !EquippedWeapon->IsFull() && !bLocallyReloading)
	{
		ServerReload();
		HandleReload();
		bLocallyReloading = true;
	}
}

// This is called on Server
// Changing CombatState which is replicated will cause OnRep_CombatState
// to be called on server and clients
void UCombatComponent::ServerReload_Implementation()
{
	if (Character == nullptr || EquippedWeapon == nullptr) { return; }

	CombatState = ECombatState::ECS_Reloading;
	HandleReload();
}

void UCombatComponent::FinishReloading()
{
	if (Character == nullptr) return;
	bLocallyReloading = false;
	if (Character->HasAuthority())
	{
		CombatState = ECombatState::ECS_Unoccupied;
		UpdateAmmoValues();
	}
	if (bFireButtonPressed)
	{
		Fire();
	}
}

void UCombatComponent::UpdateAmmoValues()
{
	if (Character == nullptr || EquippedWeapon == nullptr) return;
	int32 ReloadAmount = AmountToReload();
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(ReloadAmount);
}

void UCombatComponent::UpdateShotgunAmmoValues()
{
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= 1;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	EquippedWeapon->AddAmmo(1);
	bCanFire = true;
	if (EquippedWeapon->IsFull() || CarriedAmmo == 0)
	{
		JumpToShotgunEnd();
	}
}

void UCombatComponent::OnRep_Grenades()
{
	UpdateHUDGrenades();
}

void UCombatComponent::JumpToShotgunEnd()
{
	// Jump to ShotgunEnd section
	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (AnimInstance && Character->GetReloadMontage())
	{
		AnimInstance->Montage_JumpToSection(FName("ShotgunEnd"));
	}
}

void UCombatComponent::ThrowGrenadeFinished()
{
	CombatState = ECombatState::ECS_Unoccupied;
	AttachActorToRightHand(EquippedWeapon);
}

void UCombatComponent::LaunchGrenade()
{
	ShowAttachedGrenade(false);
	if (Character && Character->IsLocallyControlled())
	{
		ServerLaunchGrenade(HitTarget);
	}
}

void UCombatComponent::ServerLaunchGrenade_Implementation(const FVector_NetQuantize& Target)
{
	if (Character && GrenadeClass && Character->GetAttachedGrenade())
	{
		const FVector StartingLocation = Character->GetAttachedGrenade()->GetComponentLocation();
		FVector ToTarget = Target - StartingLocation;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Character;
		SpawnParams.Instigator = Character;
		UWorld* World = GetWorld();
		if (World)
		{
			World->SpawnActor<AProjectile>(
				GrenadeClass,
				StartingLocation,
				ToTarget.Rotation(),
				SpawnParams
				);
		}
	}
}

void UCombatComponent::OnRep_CombatState()
{
	switch (CombatState)
	{
	case ECombatState::ECS_Reloading:
		HandleReload();
		break;
		// Case for handling an edge case that reloading animation is finished but combat state was
		// not yet replicated which does not allow a client to fire if he is pressing a button
	case ECombatState::ECS_Unoccupied:
		if (bFireButtonPressed)
		{
			Fire();
		}
		break;
	case ECombatState::ECS_ThrowingGrenade:
		if (Character && !Character->IsLocallyControlled())
		{
			Character->PlayThrowGrenadeMontage();
			AttachActorToLeftHand(EquippedWeapon);
			ShowAttachedGrenade(true);
		}
		break;
	}
}


void UCombatComponent::HandleReload()
{
	if (Character)
	{
		Character->PlayReloadMontage();
	}
}

int32 UCombatComponent::AmountToReload()
{
	if (EquippedWeapon == nullptr) return 0;
	int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();

	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType()))
	{
		int32 AmountCarried = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
		int32 Least = FMath::Min(RoomInMag, AmountCarried);
		return FMath::Clamp(RoomInMag, 0, Least);
	}
	return 0;
}

void UCombatComponent::ThrowGrenade()
{
	if (Grenades == 0) return;
	if (CombatState != ECombatState::ECS_Unoccupied || EquippedWeapon == nullptr) return;
	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (Character)
	{
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		ShowAttachedGrenade(true);
	}
	if (Character && !Character->HasAuthority())
	{
		ServerThrowGrenade();
	}
	if (Character && Character->HasAuthority())
	{
		Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
		UpdateHUDGrenades();
	}
}

void UCombatComponent::ServerThrowGrenade_Implementation()
{
	if (Grenades == 0) return;
	CombatState = ECombatState::ECS_ThrowingGrenade;
	if (Character)
	{
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		ShowAttachedGrenade(true);
	}
	Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
	UpdateHUDGrenades();
}

void UCombatComponent::UpdateHUDGrenades()
{
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		Controller->SetHUDGrenades(Grenades);
	}
}

void UCombatComponent::ShowAttachedGrenade(bool bShowGrenade)
{
	if (Character && Character->GetAttachedGrenade())
	{
		Character->GetAttachedGrenade()->SetVisibility(bShowGrenade);
	}
}

void UCombatComponent::OnRep_EquippedWeapon(AWeapon* LastWeapon)
{
	
}

void UCombatComponent::OnRep_BackpackWeapon1(AWeapon* LastWeapon)
{
	if (BackpackWeapon1 && Character)
	{
		{
			BackpackWeapon1->SetWeaponState(EWeaponState::EWS_Equipped);
			AttachActorToRightHand(BackpackWeapon1);
		}

		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;

		PlayEquipWeaponSound(BackpackWeapon1);
		BackpackWeapon1->SetHUDAmmo(); 
		BackpackWeapon1->SetHUDWeaponSlot(); 
	}

}

void UCombatComponent::OnRep_BackpackWeapon2(AWeapon* LastWeapon)
{
	if (BackpackWeapon2)
	{
		// Copied from EquipWeapon - making sure that client is properly set up
		{
			BackpackWeapon2->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			// This is automatically replicated
			AttachActorToBackpack(BackpackWeapon2, FName("BackpackSocket2"));
		}

		PlayEquipWeaponSound(BackpackWeapon2);
		BackpackWeapon2->SetHUDWeaponSlot(); // To update it on clients
	}
}

void UCombatComponent::OnRep_BackpackWeapon3(AWeapon* LastWeapon)
{
	if (BackpackWeapon3)
	{
		// Copied from EquipWeapon - making sure that client is properly set up
		{
			BackpackWeapon3->SetWeaponState(EWeaponState::EWS_EquippedOnBackpack);
			// This is automatically replicated
			AttachActorToBackpack(BackpackWeapon3, FName("BackpackSocket3"));
		}

		PlayEquipWeaponSound(BackpackWeapon3);
		BackpackWeapon3->SetHUDWeaponSlot(); // To update it on clients
	}
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection
	);

	if (bScreenToWorld)
	{
		FVector Start = CrosshairWorldPosition;

		if (Character)
		{
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			Start += CrosshairWorldDirection * (DistanceToCharacter + 100.f);
		}

		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;

		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult,
			Start,
			End,
			ECollisionChannel::ECC_Visibility
		);
		if (TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>())
		{
			HUDPackage.CrosshairsColor = FLinearColor::Red;
		}
		else
		{
			HUDPackage.CrosshairsColor = FLinearColor::White;
		}
	}
}

void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
	if (Character == nullptr || Character->Controller == nullptr) return;

	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		HUD = HUD == nullptr ? Cast<ABlasterHUD>(Controller->GetHUD()) : HUD;
		if (HUD)
		{
			if (EquippedWeapon)
			{
				HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
				HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
				HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
				HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;
				HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
			}
			else
			{
				HUDPackage.CrosshairsCenter = nullptr;
				HUDPackage.CrosshairsLeft = nullptr;
				HUDPackage.CrosshairsRight = nullptr;
				HUDPackage.CrosshairsBottom = nullptr;
				HUDPackage.CrosshairsTop = nullptr;
			}
			// Calculate crosshair spread

			// [0, 600] -> [0, 1]
			FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			FVector2D VelocityMultiplierRange(0.f, 1.f);
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;

			CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplierRange, Velocity.Size());

			if (Character->GetCharacterMovement()->IsFalling())
			{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
			}
			else
			{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 30.f);
			}

			if (bAiming)
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.58f, DeltaTime, 30.f);
			}
			else
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
			}

			CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 40.f);

			HUDPackage.CrosshairSpread =
				0.5f +
				CrosshairVelocityFactor +
				CrosshairInAirFactor -
				CrosshairAimFactor +
				CrosshairShootingFactor;

			HUD->SetHUDPackage(HUDPackage);
		}
	}
}

void UCombatComponent::InterpFOV(float DeltaTime)
{
	if (EquippedWeapon == nullptr) return;

	if (bAiming)
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
	}
	else
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}
	if (Character && Character->GetFollowCamera())
	{
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}

void UCombatComponent::SetAiming(bool bIsAiming)
{
	if (Character == nullptr || EquippedWeapon == nullptr) return;
	bAiming = bIsAiming;
	ServerSetAiming(bIsAiming);
	if (Character)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
	if (Character->IsLocallyControlled() && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle)
	{
		Character->ShowSniperScopeWidget(bIsAiming);
	}
	if (Character->IsLocallyControlled()) bAimButtonPressed = bIsAiming;
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;
	if (Character)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

bool UCombatComponent::CanFire()
{
	if (EquippedWeapon == nullptr) return false;
	if (!EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Reloading && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun) return true;
	if (bLocallyReloading) return false;
	return !EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Unoccupied;
}

void UCombatComponent::OnRep_CarriedAmmo()
{
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
	if (Controller)
	{
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	bool bJumpToShotgunEnd =
		CombatState == ECombatState::ECS_Reloading &&
		EquippedWeapon != nullptr &&
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun &&
		CarriedAmmo == 0;
	if (bJumpToShotgunEnd)
	{
		JumpToShotgunEnd();
	}
}

void UCombatComponent::InitializeCarriedAmmo()
{
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
}