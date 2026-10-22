// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/PDDinosaurBase.h"
#include "ACFMountComponent.h"
#include "ACFGASStatisticsComponent.h"
#include "Actors/ACFCharacter.h"
#include "Components/ACFQuadrupedMovementComponent.h"
#include "ACMCollisionManagerComponent.h"
#include "ACFVaultComponent.h"
#include "ALSLoadAndSaveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "EditorCategoryUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/PDBreedableInterface.h"
#include "Interfaces/PDTameableInterface.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/PangeaCharacterInitializerComponent.h"
#include "Components/PangeaGASStatisticsComponent.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/UnrealType.h"
#include "Net/UnrealNetwork.h"


APDDinosaurBase::APDDinosaurBase(const FObjectInitializer& ObjectInitializer)
	: Super(
		ObjectInitializer
			.SetDefaultSubobjectClass<UACFQuadrupedMovementComponent>(ACharacter::CharacterMovementComponentName)
			.SetDefaultSubobjectClass<UPangeaCharacterInitializerComponent>(TEXT("Character Initializer"))
			.SetDefaultSubobjectClass<UPangeaGASStatisticsComponent>(TEXT("Statistic Component")))
			
{
	MountComponent = CreateDefaultSubobject<UACFMountComponent>(TEXT("ACF Mount Component"));
	VaultComponent = CreateDefaultSubobject<UACFVaultComponent>(TEXT("ACF Vault Component"));
	ALSLoadAndSaveComponent = CreateDefaultSubobject<UALSLoadAndSaveComponent>(TEXT("ALS Load And Save Component"));

	ConfigureCombatCollision();
	SetCanBeDamaged(true);

	if (UACMCollisionManagerComponent* CollisionManager = GetCollisionsComponent())
	{
		CollisionManager->AddCollisionChannel(ECC_GameTraceChannel1);
	}
}

void APDDinosaurBase::PreInitializeComponents()
{
	if (HasAuthority() && !IsTemplate() && !CreatureId.IsValid()) CreatureId = FGuid::NewGuid();
	EnsureDefaultCombatStatRow(false);
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void APDDinosaurBase::BeginPlay()
{
	Super::BeginPlay();
	EnsureDefaultCombatStatRow(true);
	// ACF profile fragments may deliberately override collision. Only legacy creatures
	// need the old post-BeginPlay correction; constructor defaults still apply to all.
	if (!GetInitializerComponent() || !GetInitializerComponent()->GetCharacterDataAsset())
		ConfigureCombatCollision();
	SetCanBeDamaged(true);
	
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		this, 
		UGameFrameworkComponentManager::NAME_GameActorReady);
}

void APDDinosaurBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}



void APDDinosaurBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ChangeVelocityState();
}

#pragma region Save System

TArray<UActorComponent*> APDDinosaurBase::GetComponentsToSave_Implementation() const
{
	// Preserve ACF's stats, damage state and equipment as well as feature state.
	TArray<UActorComponent*> ComponentsToSave = Super::GetComponentsToSave_Implementation();

	if (UActorComponent* BreedableComponent = FindComponentByInterface(UPDBreedableInterface::StaticClass()))
	{
		ComponentsToSave.AddUnique(BreedableComponent);
	}
	
	if (UActorComponent* TamingComponent = FindComponentByInterface(UPDTameableInterface::StaticClass()))
	{
		ComponentsToSave.AddUnique(TamingComponent);
	}

	return ComponentsToSave;
}

#pragma endregion

#pragma region ACF Interaction Interface

bool APDDinosaurBase::CanBeInteracted_Implementation(class APawn* Pawn)
{
	UActorComponent* TamingComponent = FindComponentByInterface(UPDTameableInterface::StaticClass());
	if (!TamingComponent) return false;
	
	IPDTameableInterface* Tameable = Cast<IPDTameableInterface>(TamingComponent);
	
	return !MountComponent->IsMounted() && Tameable->CanBeTamed();
}

void APDDinosaurBase::OnInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType)
{
	UActorComponent* TamingComponent = FindComponentByInterface(UPDTameableInterface::StaticClass());
    
	if (!TamingComponent)
	{
		return;
	}
	
	IPDTameableInterface* Tameable = Cast<IPDTameableInterface>(TamingComponent);
	AACFCharacter* ACFCharacter = Cast<AACFCharacter>(Pawn);

	if (Tameable->GetTameState() != ETameState::Tamed)
	{
		// Not tamed yet → maybe start a taming attempt instead
		Tameable->StartTameAttempt(Pawn);
		return;
	}

	if (Tameable->GetTamedRole() == ETamedRole::Mount)
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
	else if (Tameable->GetTamedRole() == ETamedRole::Companion)
	{
		UE_LOG(LogEngine, Display, TEXT("You Pet Dino!"));
	}
}

FText APDDinosaurBase::GetInteractableName_Implementation()
{
	UActorComponent* TamingComponent = FindComponentByInterface(UPDTameableInterface::StaticClass());
	if (!TamingComponent) return FText::FromString("Interact");
	
	IPDTameableInterface* Tameable = Cast<IPDTameableInterface>(TamingComponent);
	
	if (Tameable->GetTameState() == ETameState::Wild)
	{
		return FText::FromString("Tame");
	}
	
	if (Tameable->GetTameState() == ETameState::Tamed)
	{
		if (Tameable->GetTamedRole() == ETamedRole::Mount) return FText::FromString("Mount");
		if (Tameable->GetTamedRole() == ETamedRole::Companion) return FText::FromString("Pet");
	}
	
	return FText::FromString("Interact");
}

UPangeaCreatureDefinition* APDDinosaurBase::GetCreatureDefinition_Implementation() const
{
	return CreatureDefinition;
}

bool APDDinosaurBase::SetCreatureDefinition_Implementation(UPangeaCreatureDefinition* Definition)
{
	// This is spawn configuration, not an API to reset a living/saved creature.
	if (!HasAuthority() || IsTemplate() || IsActorInitialized() || HasActorBegunPlay() || !Definition ||
		(GetInitializerComponent() && GetInitializerComponent()->HasBeenInitialized())) return false;
	const UClass* SpawnClass = Definition->CreatureClass.LoadSynchronous();
	if (!SpawnClass || !IsA(SpawnClass)) return false;
	CreatureDefinition = Definition;
	return true;
}

void APDDinosaurBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APDDinosaurBase, CreatureDefinition);
	DOREPLIFETIME(APDDinosaurBase, CreatureId);
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

void APDDinosaurBase::OnLoaded_Implementation()
{
	Super::OnLoaded_Implementation();
	if (IsActorBeingDestroyed())
	{
		return;
	}
	if (HasAuthority())
	{
		if (!CreatureId.IsValid()) CreatureId = FGuid::NewGuid(); // Legacy records have no historical identity to restore.
		ForceNetUpdate();
	}
	if (UActorComponent* TamingComponent = FindComponentByInterface(UPDTameableInterface::StaticClass()))
	{
		IPDTameableInterface* Tameable = Cast<IPDTameableInterface>(TamingComponent);
		
		Tameable->HandleLoadedActor();
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

void APDDinosaurBase::EnsureDefaultCombatStatRow(bool bReinitializeIfAssigned)
{
	// A configured profile owns baseline attributes. Do not inject sample data or
	// run an additional stat initialization before/after ACF's profile lifecycle.
	if (UACFCharacterInitializerComponent* Initializer = GetInitializerComponent())
	{
		if (Initializer->GetAutoInitDataAsset() || Initializer->GetCharacterDataAsset()) return;
		if (const UPangeaCharacterInitializerComponent* PangeaInitializer = Cast<UPangeaCharacterInitializerComponent>(Initializer))
			if (PangeaInitializer->GetEffectiveCharacterProfile()) return;
	}

	UACFGASStatisticsComponent* StatisticsComponent = FindComponentByClass<UACFGASStatisticsComponent>();
	if (!StatisticsComponent)
	{
		return;
	}

	FStructProperty* CharacterRowProperty = FindFProperty<FStructProperty>(StatisticsComponent->GetClass(), TEXT("CharacterRow"));
	if (!CharacterRowProperty)
	{
		return;
	}

	FDataTableRowHandle* CharacterRow = CharacterRowProperty->ContainerPtrToValuePtr<FDataTableRowHandle>(StatisticsComponent);
	if (!CharacterRow || CharacterRow->DataTable || !CharacterRow->RowName.IsNone())
	{
		return;
	}

	UDataTable* DefaultAttributesTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/FullSample/Blueprints/GAS/ACF_SampleAttributesInit_DT.ACF_SampleAttributesInit_DT"));
	if (!DefaultAttributesTable)
	{
		return;
	}

	CharacterRow->DataTable = DefaultAttributesTable;
	CharacterRow->RowName = TEXT("MMEnemy");
	UE_LOG(LogTemp, Warning, TEXT("%s is using the legacy FullSample/MMEnemy stat fallback. Configure an ACF Character Profile to migrate it."), *GetNameSafe(this));

	if (bReinitializeIfAssigned)
	{
		StatisticsComponent->InitializeAttributeSet();
	}
}

void APDDinosaurBase::ConfigureCombatCollision()
{
	if (UCapsuleComponent* CharacterCapsule = GetCapsuleComponent())
	{
		CharacterCapsule->SetCollisionProfileName(TEXT("Pawn"));
		CharacterCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CharacterCapsule->SetCollisionObjectType(ECC_Pawn);
		CharacterCapsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetCollisionProfileName(TEXT("CharacterMesh"));
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetGenerateOverlapEvents(false);
	}
}

#pragma endregion
