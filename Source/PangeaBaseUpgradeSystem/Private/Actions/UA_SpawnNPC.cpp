// Fill out your copyright notice in the Description page of Project Settings.


#include "Actions/UA_SpawnNPC.h"

#include "GameplayTagAssetInterface.h"
#include "Components/FacilityManagerComponent.h"

void UUA_SpawnNPC::Execute_Implementation(UObject* ContextObject)
{
	AActor* Owner = Cast<AActor>(ContextObject);
	if (!Owner || !NPCClass)
	{
		return;
	}

	FVector SpawnLocation = SpawnTransform.GetLocation();
	FRotator SpawnRotation = SpawnTransform.Rotator();

	if (SpawnMarkerTag.IsValid())
	{
		// Get facility manager from owner
		UFacilityManagerComponent* FacilityManager = Owner->FindComponentByClass<UFacilityManagerComponent>();
        
		if (FacilityManager)
		{
			AActor* Marker = FacilityManager->GetFacilityMarker(SpawnMarkerTag);
			if (Marker)
			{
				SpawnLocation = Marker->GetActorLocation();
				SpawnRotation = Marker->GetActorRotation();
				UE_LOG(LogTemp, Log, TEXT("UA_SpawnNPC: Using marker at %s"), *SpawnLocation.ToString());
			}
		}
	}

	// Spawn NPC at location
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* SpawnedNPC = Owner->GetWorld()->SpawnActor<APawn>(NPCClass, SpawnLocation, SpawnRotation, SpawnParams);
}
