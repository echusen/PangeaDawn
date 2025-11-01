// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/PDDinosaurBase.h"
#include "Components/PangeaBreedableComponent.h"
#include "ACFMountComponent.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFQuadrupedMovementComponent.h"
#include "ACFVaultComponent.h"
#include "Components/PangeaTamingComponent.h"
#include "DataAssets/TameSpeciesConfig.h"



APDDinosaurBase::APDDinosaurBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	BreedableComponent = CreateDefaultSubobject<UPangeaBreedableComponent>(TEXT("Pangea Breeding Component"));
	MountComponent = CreateDefaultSubobject<UACFMountComponent>(TEXT("ACF Mount Component"));
	VaultComponent = CreateDefaultSubobject<UACFVaultComponent>(TEXT("ACF Vault Component"));
	TamingComponent = CreateDefaultSubobject<UPangeaTamingComponent>(TEXT("Pangea Taming Component"));
}

void APDDinosaurBase::BeginPlay()
{
	Super::BeginPlay();
}

void APDDinosaurBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ChangeVelocityState();
}

#pragma region ACF Interaction Interface

bool APDDinosaurBase::CanBeInteracted_Implementation(class APawn* Pawn)
{
	// Can be interacted with if not mounted and if can be a mount or companion 
	return !MountComponent->IsMounted() && TamingComponent && TamingComponent->TameSpeciesConfig &&
		(TamingComponent->TameSpeciesConfig->bCanBeMount || TamingComponent->TameSpeciesConfig->bCanBeCompanion);
}

void APDDinosaurBase::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	AACFCharacter* ACFCharacter = Cast<AACFCharacter>(Pawn);

	if (!TamingComponent || TamingComponent->TameState != ETameState::Tamed)
	{
		// Not tamed yet → maybe start a taming attempt instead
		TamingComponent->StartTameAttempt(Pawn);
		return;
	}

	if (TamingComponent->TamedRole == ETamedRole::Mount)
	{
		//request gameplay tag
		FGameplayTag MountTag = FGameplayTag::RequestGameplayTag("Actions.Mount");
		FGameplayTag DismountTag = FGameplayTag::RequestGameplayTag("Actions.Dismount");
	
		if (!ACFCharacter) return;

		if (MountComponent->IsMounted())
		{
			ACFCharacter->TriggerAction(DismountTag, EActionPriority::ELow, false);
		}
		else
		{
			ACFCharacter->TriggerAction(MountTag, EActionPriority::EHigh, false);
		}
	}
	else if (TamingComponent->TamedRole == ETamedRole::Companion)
	{
		UE_LOG(LogEngine, Display, TEXT("You Pet Dino!"));
	}
}

FText APDDinosaurBase::GetInteractableName_Implementation()
{
	// return Tame if not tamed and once tamed return mount/pet based on role
	if (TamingComponent && TamingComponent->TameState == ETameState::Wild)
	{
		return FText::FromString("Tame");
	}

	if (TamingComponent && TamingComponent->TameState == ETameState::Tamed)
	{
		if (TamingComponent->TamedRole == ETamedRole::Mount)
		{
			return FText::FromString("Mount");
		}

		if (TamingComponent->TamedRole == ETamedRole::Companion)
		{
			return FText::FromString("Pet");
		}
	}
	
	return FText::FromString("Interact");
}
#pragma endregion

#pragma region Private Movement Functions

void APDDinosaurBase::Accelerate(float Value)
{
	//cast movement component to ACFQuadrupedMovementComponent
	UACFQuadrupedMovementComponent* QuadMovementComp = Cast<UACFQuadrupedMovementComponent>(GetMovementComponent());
	if (QuadMovementComp)
	{
		QuadMovementComp->MoveForwardLocal(Value);
	}
}

void APDDinosaurBase::Brake(float Value)
{
	UACFQuadrupedMovementComponent* QuadMovementComp = Cast<UACFQuadrupedMovementComponent>(GetMovementComponent());
	if (QuadMovementComp)
	{
		QuadMovementComp->MoveForwardLocal(Value);
	}
}

void APDDinosaurBase::ChangeVelocityState()
{
	if (bIsAccelerating)
	{
		Accelerate(DefaultAcceleration);
	}
	else if (bIsBraking)
	{
		Brake(DefaultDeceleration);
	}
}

#pragma endregion