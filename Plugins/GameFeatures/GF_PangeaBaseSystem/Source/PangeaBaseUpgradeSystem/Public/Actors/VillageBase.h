// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ALSSavableInterface.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ACFInteractableInterface.h"
#include "VillageBase.generated.h"

class APawn;
class UActorComponent;
class UBoxComponent;
class UPangeaFacilityManagerComponent;
class UPangeaUpgradeSystemComponent;
class UVillageUpgradeMenuWidget;

/**
 * Actor that represents an upgradable base (village) in the world.
 * It is the interaction point that opens the upgrade menu, and the save root for the upgrade and facility
 * components that a game feature adds to it.
 */
UCLASS()
class PANGEABASEUPGRADESYSTEM_API AVillageBase : public AActor, public IACFInteractableInterface, public IALSSavableInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVillageBase();

protected:
	virtual void PreInitializeComponents() override;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;
	virtual void OnLoaded_Implementation() override;

public:
	/** Defines the spatial size of the base */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Village")
	TObjectPtr<UBoxComponent> VillageBounds;

	/** Player interaction zone for upgrading */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Village")
	TObjectPtr<UBoxComponent> InteractionVolume;

	// -----------------------------
	// ACFU INTERACTION INTERFACE
	// -----------------------------

	/** Text that appears when player looks at interaction point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionText;

	/** Triggered when player interacts (ACFU interface) */
	virtual void OnLocalInteractedByPawn_Implementation(APawn* Pawn, const FString& interactionType = TEXT("")) override;

	/** Interaction Name for UI */
	virtual FText GetInteractableName_Implementation() override;

	/** Interaction allowed? */
	virtual bool CanBeInteracted_Implementation(APawn* Pawn) override;

	/** Asks the upgrade provider component to upgrade to the next level for InstigatorPawn. Returns true when the level was committed. */
	bool UpgradeBase(APawn* InstigatorPawn) const;

	//UI Integration
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade|UI")
	TSubclassOf<UVillageUpgradeMenuWidget> UpgradeMenuClass;

	/** Opens the village page for the local player that controls InteractingPawn and binds its upgrade content to this village. */
	UFUNCTION(BlueprintCallable, Category = "Upgrade|UI")
	void OpenUpgradeMenu(APawn* InteractingPawn);

	/** Returns the upgrade component on this village, or null when the upgrade feature has not added one. */
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	UPangeaUpgradeSystemComponent* GetUpgradeSystem() const;

	/** Returns the facility manager on this village, or null when there is none. */
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	UPangeaFacilityManagerComponent* GetFacilityManager() const;

private:
	/** Returns the first component that implements the base upgrade interface, or null. */
	UActorComponent* GetUpgradeProviderComponent() const;

	/** Returns true when an upgrade component with a definition is present. Interaction is off without it. */
	bool HasActiveUpgradeFeature() const;
};
