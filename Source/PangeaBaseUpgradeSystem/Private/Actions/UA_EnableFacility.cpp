// Fill out your copyright notice in the Description page of Project Settings.


#include "Actions/UA_EnableFacility.h"

#include "Components/FacilityManagerComponent.h"

void UUA_EnableFacility::Execute_Implementation(UObject* ContextObject)
{
	AActor* Owner = Cast<AActor>(ContextObject);
	if (!Owner)
		return;

	UFacilityManagerComponent* Manager = Owner->FindComponentByClass<UFacilityManagerComponent>();
	if (!Manager)
		return;

	// Enable facility (handles existing actors)
	if (EnableMode == EFacilityEnableMode::RevealExisting || EnableMode == EFacilityEnableMode::Both)
	{
		Manager->EnableFacility(FacilityTag);
	}

	// Spawn new NPCs
	if (EnableMode == EFacilityEnableMode::SpawnNew || EnableMode == EFacilityEnableMode::Both)
	{
		AActor* Marker = Manager->GetFacilityMarker(FacilityTag);
		if (Marker)
		{
			for (TSubclassOf<APawn> NPCClass : NPCsToSpawn)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = Owner;
                
				Owner->GetWorld()->SpawnActor<APawn>(
					NPCClass,
					Marker->GetActorLocation(),
					Marker->GetActorRotation(),
					SpawnParams
				);
			}
		}
	}
}
