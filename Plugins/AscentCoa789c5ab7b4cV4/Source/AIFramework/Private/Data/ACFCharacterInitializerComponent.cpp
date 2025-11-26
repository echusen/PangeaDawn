// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/ACFCharacterInitializerComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Logging.h"
#include "Data/ACFCharacterDataAsset.h"
#include "Components/ACFTeamComponent.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Components/ACFInventoryComponent.h"
#include "ACFGASAttributesComponent.h"
#include "Components/ACFCombatBehaviourComponent.h"
#include <AIController.h>
#include "Actors/ACFCharacter.h"
#include "ACFAITypes.h"
#include <Components/SkeletalMeshComponent.h>
#include <Animation/AnimInstance.h>

// Sets default values for this component's properties
UACFCharacterInitializerComponent::UACFCharacterInitializerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	bHasInitialized = false;
	// ...
}

// Called when the game starts
void UACFCharacterInitializerComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningPawn = Cast<AACFCharacter>(GetOwner());
	// Handle auto-initialization if enabled
	if (bAutoInit && AutoInitDataAsset.IsValid() && GetOwnerRole() == ROLE_Authority)
	{
		// Check if asset is already loaded
		if (UACFCharacterDataAsset* LoadedAsset = AutoInitDataAsset.Get())
		{
			// Asset is already in memory, initialize immediately
			InitFromDataAsset(LoadedAsset, AutoInitLevel);
		}
		else
		{
			// Need to load the asset first
			UAssetManager& AssetManager = UAssetManager::Get();
			FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();

			// Request async load
			LoadHandle = StreamableManager.RequestAsyncLoad(
				AutoInitDataAsset.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([this]()
					{
						if (UACFCharacterDataAsset* LoadedAsset = AutoInitDataAsset.Get())
						{
							InitFromDataAsset(LoadedAsset, AutoInitLevel);
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("Failed to load auto-init data asset"));
						}
					}),
				FStreamableManager::AsyncLoadHighPriority);

			UE_LOG(LogTemp, Log, TEXT("Auto-loading character data asset for initialization"));
		}
	}
}

void UACFCharacterInitializerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UACFCharacterInitializerComponent, CharacterDataAssetId);
}



void UACFCharacterInitializerComponent::InitFromDataAsset(UACFCharacterDataAsset* charData, int32 Level)
{
	if (!charData)
	{
		UE_LOG(ACFAILog, Warning, TEXT("InitFromDataAsset called with null DataAsset"));
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(ACFAILog, Warning, TEXT("InitFromDataAsset should be called only on server"));
		return;
	}

	if (bHasInitialized) {
		UE_LOG(ACFAILog, Warning, TEXT("InitFromDataAsset called but component is already initialized"));
		return;
	}
	CharacterDataAsset = charData;
	//Trigger replication
	CharacterDataAssetId = charData->GetPrimaryAssetId();

	InternalHandleServerInit(Level);
}



void UACFCharacterInitializerComponent::OnRep_CharacterDataAssetId()
{
	// Called on clients when CharacterDataAssetId is replicated
	if (CharacterDataAssetId.IsValid())
	{
		LoadDataAssetFromId();
	}
}


void UACFCharacterInitializerComponent::InternalHandleServerInit(int32 Level)
{
	ApplyAllMeshData();

	if (OwningPawn && CharacterDataAsset) {
		UACFGASAttributesComponent* AttributesComp = OwningPawn->FindComponentByClass<UACFGASAttributesComponent>();

		if (AttributesComp) {
			AttributesComp->SetLevelingType(CharacterDataAsset->LevelingType);
			AttributesComp->SetCharacterRow(CharacterDataAsset->CharacterRow);
			AttributesComp->ForceSetLevel(Level);
			AttributesComp->InitializeAttributeSet();
		}

		// Initialize inventory with starting items
		UACFEquipmentComponent* InventoryComp = OwningPawn->FindComponentByClass<UACFEquipmentComponent>();
		if (InventoryComp) {
			InventoryComp->SetStartingItems(CharacterDataAsset->StartingItems);
		}
		UACFAbilitySystemComponent* AbilityComp = OwningPawn->FindComponentByClass<UACFAbilitySystemComponent>();

		if (AbilityComp) {
			//AbilityComp->ClearAllAbilities();
			AbilityComp->GrantAbilitySet(CharacterDataAsset->DefaultAbilitySet, FGameplayTag());
			for (const auto& AbilitySet : CharacterDataAsset->MovesetAbilities) {
				AbilityComp->GrantAbilitySet(AbilitySet.Value, AbilitySet.Key);
			}
		}

		UACFTeamComponent* teamComp = OwningPawn->FindComponentByClass<UACFTeamComponent>();
		if (teamComp) {
			teamComp->SetTeam(CharacterDataAsset->Team);
		}

		AAIController* AICont = Cast<AAIController>(OwningPawn->GetController());
		if (AICont) {
			UACFCombatBehaviourComponent* CombatComp = AICont->FindComponentByClass<UACFCombatBehaviourComponent>();
			if (CombatComp) {
				CombatComp->SetCombatBehaviour(CharacterDataAsset->CombatBehaviour);
			}
		}
		if (CharacterDataAsset->CharacterPortrait)
		{
			OwningPawn->SetCharacterPortrait(CharacterDataAsset->CharacterPortrait);
		}
		OwningPawn->SetCharacterName(CharacterDataAsset->ChatacterName);
	}
	HandleServerInit();
	bHasInitialized = true;
}

void UACFCharacterInitializerComponent::InternalHandleClientInit()
{
	ApplyAllMeshData();
	HandleClientInit();
	bHasInitialized = true;
}

void UACFCharacterInitializerComponent::HandleServerInit_Implementation()
{

}

void UACFCharacterInitializerComponent::HandleClientInit_Implementation()
{

}

void UACFCharacterInitializerComponent::LoadDataAssetFromId()
{
	if (!CharacterDataAssetId.IsValid())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	// Asset needs to be loaded asynchronously
	FStreamableManager& StreamableManager = AssetManager.GetStreamableManager();
	FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(CharacterDataAssetId);

	if (AssetPath.IsValid())
	{
		StreamableManager.RequestAsyncLoad(AssetPath,
			FStreamableDelegate::CreateUObject(this,
				&UACFCharacterInitializerComponent::OnDataAssetLoaded));
	}
}

void UACFCharacterInitializerComponent::OnDataAssetLoaded()
{
	// Try again to get the loaded asset
	UAssetManager& AssetManager = UAssetManager::Get();
	CharacterDataAsset = Cast<UACFCharacterDataAsset>(
		AssetManager.GetPrimaryAssetObject(CharacterDataAssetId));

	InternalHandleClientInit();
}

void UACFCharacterInitializerComponent::ApplyAllMeshData()
{
	if (!CharacterDataAsset)
	{
		return;
	}

	// Get all skeletal mesh components from the character
	TArray<USkeletalMeshComponent*> SkeletalComponents;
	GetOwner()->GetComponents<USkeletalMeshComponent>(SkeletalComponents);
	
	// Apply mesh data to components with matching tags
	for (const FSkeletalMeshComponentData& MeshData : CharacterDataAsset->MeshComponents)
	{
		if (MeshData.ComponentTag == NAME_None) {
			const UACFEquipmentComponent* equipComp = GetOwner()->FindComponentByClass<UACFEquipmentComponent>();
			if (equipComp) {
				USkeletalMeshComponent* meshComp = equipComp->GetMainMesh();
				ApplyMeshDataToComponent(meshComp, MeshData);
			}
			continue;
		}
		// Find component with matching tag
		for (USkeletalMeshComponent* Component : SkeletalComponents)
		{

			if (Component->ComponentHasTag(MeshData.ComponentTag))
			{
				ApplyMeshDataToComponent(Component, MeshData);
				break;
			}

		}
	}
}


void UACFCharacterInitializerComponent::ApplyMeshDataToComponent(USkeletalMeshComponent* Component,
	const FSkeletalMeshComponentData& MeshData)
{
	if (!Component || !MeshData.SkeletalMesh)
	{
		return;
	}
	Component->SetSkeletalMesh(MeshData.SkeletalMesh);
	if (MeshData.AnimInstance) {
		Component->SetAnimInstanceClass(MeshData.AnimInstance);
	}
	// Apply materials
	for (int32 i = 0; i < MeshData.MaterialOverrides.Num(); ++i)
	{
		if (MeshData.MaterialOverrides.IsValidIndex(i))
		{
			Component->SetMaterial(i, MeshData.MaterialOverrides[i].Material);
		}
	}

}