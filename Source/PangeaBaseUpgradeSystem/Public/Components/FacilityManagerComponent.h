// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "FacilityManagerComponent.generated.h"

class AFacilityMarker;

USTRUCT(BlueprintType)
struct FFacilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag FacilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bUnlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<AActor> FacilityActor;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PANGEABASEUPGRADESYSTEM_API UFacilityManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Facilities")
	TArray<FFacilityEntry> Facilities;

	/** Unlock by gameplay tag */
	UFUNCTION(BlueprintCallable, Category="Facilities")
	void EnableFacility(FGameplayTag FacilityTag);

	UFUNCTION(BlueprintCallable, Category="Facilities")
	void DisableFacility(FGameplayTag FacilityTag);
	FGameplayTagContainer GetActorGameplayTags(AActor* Actor);

	/** Find facility entry using gameplay tag */
	bool GetFacility(FGameplayTag FacilityTag, FFacilityEntry*& OutEntry);
	
	UFUNCTION(BlueprintCallable, Category = "Facility")
	void DiscoverFacilityMarkersInWorld();

	UFUNCTION(BlueprintCallable, Category = "Facility")
	AActor* GetFacilityMarker(FGameplayTag FacilityTag) const;
	
	void RegisterFacilityMarker(AFacilityMarker* Marker);
	
	UFUNCTION(BlueprintCallable, Category = "Facility")
	bool IsFacilityEnabled(FGameplayTag FacilityTag) const;
	
	UFUNCTION(BlueprintCallable, Category = "Facility")
	void RefreshNPCStates();
	
private:
	// Store discovered markers
	UPROPERTY()
	TMap<FGameplayTag, AActor*> FacilityMarkers;
	
	TMap<FGameplayTag, TArray<TWeakObjectPtr<AActor>>> FacilityNPCs;
};
