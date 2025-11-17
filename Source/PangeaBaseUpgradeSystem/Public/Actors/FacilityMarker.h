// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "FacilityMarker.generated.h"

UCLASS()
class PANGEABASEUPGRADESYSTEM_API AFacilityMarker : public AActor, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AFacilityMarker();

	/** Tag representing this facility (e.g. Village.Facility.Blacksmith) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Facility")
	FGameplayTag FacilityTag;
	
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
};
