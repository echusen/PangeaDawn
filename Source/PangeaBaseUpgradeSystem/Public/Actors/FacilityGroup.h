// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "FacilityGroup.generated.h"


class UFacilitySlotComponent;


UCLASS()
class PANGEABASEUPGRADESYSTEM_API AFacilityGroup : public AActor
{
	GENERATED_BODY()


public:
	AFacilityGroup();

	virtual void BeginPlay() override;

	/* The gameplay tag for this entire facility (e.g. Test.Village.Blacksmith) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Facility")
	FGameplayTag FacilityTag;

	/* Called by FacilityManager to enable/disable all spawned actors */
	void SetFacilityEnabled(bool bEnabled);

	/* Accessor for FacilityManager */
	const FGameplayTag& GetFacilityTag() const { return FacilityTag; }

protected:

	/* Spawn actors for all slot components */
	void SpawnAllSlots();

	/* Spawn a single slot */
	void SpawnFromSlot(UFacilitySlotComponent* Slot);

	/* All actors spawned from slots */
	UPROPERTY()
	TArray<AActor*> SpawnedActors;
};
