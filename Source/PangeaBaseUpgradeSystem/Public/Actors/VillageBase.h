// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ACFInteractableInterface.h"
#include "VillageBase.generated.h"

class UUpgradeSystemComponent;
class UFacilityManagerComponent;
class UBoxComponent;

UCLASS()
class PANGEABASEUPGRADESYSTEM_API AVillageBase : public AActor, public IACFInteractableInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVillageBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:

	/** Defines the spatial size of the base */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Village")
	UBoxComponent* VillageBounds;

	/** Player interaction zone for upgrading */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Village")
	UBoxComponent* InteractionVolume;

	/** Handles the milestone → requirement → action logic */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Village")
	UUpgradeSystemComponent* UpgradeSystem;

	/** Manages all facilities (locked/unlocked + references) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Village")
	UFacilityManagerComponent* FacilityManager;

	// -----------------------------
	// ACFU INTERACTION INTERFACE
	// -----------------------------

	/** Text that appears when player looks at interaction point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction")
	FText InteractionText;

	/** Triggered when player interacts (ACFU interface) */
	virtual void OnLocalInteractedByPawn_Implementation(class APawn* Pawn, const FString& interactionType = "") override;

	/** Interaction Name for UI */
	virtual FText GetInteractableName_Implementation() override {return InteractionText;}

	/** Interaction allowed? */
	virtual bool CanBeInteracted_Implementation(class APawn* Pawn) override {return true;};
	
	bool UpgradeBase(APawn* InstigatorPawn) const;
	
	//UI Integration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade|UI")
	TSubclassOf<class UVillageUpgradeMenuWidget> UpgradeMenuClass;

	UFUNCTION(BlueprintCallable, Category="Upgrade|UI")
	void OpenUpgradeMenu(APawn* InteractingPawn);

	UUpgradeSystemComponent* GetUpgradeSystem() const { return UpgradeSystem; }

};
