// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/PangeaFarmInterface.h"
#include "PangeaBreedingFarmComponent.generated.h"

class APangeaEggActor;
class UBoxComponent;
class UPangeaBreedableComponent;
class UPangeaBreedingFragment;
class UPangeaCreatureDefinition;
class UPangeaGeneticStrategy;

/** Broadcast on the server when a breeding attempt in the farm produced an egg. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggSpawned, APangeaEggActor*, Egg);

/**
 * Breeding logic of a farm: tracks the breedable creatures inside its zone, validates parent pairs
 * and spawns the egg on the server.
 */
UCLASS(ClassGroup = (Breeding), meta = (BlueprintSpawnableComponent))
class PANGEABREEDINGSYSTEM_API UPangeaBreedingFarmComponent : public UActorComponent, public IPangeaFarmInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPangeaBreedingFarmComponent();

	/** Overlap volume that defines which creatures are inside the farm. Taken from the owning farm actor when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breeding|References")
	TObjectPtr<UBoxComponent> BreedingZone;

	/** List of breedables currently inside the farm zone */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breeding|Farm")
	TArray<UPangeaBreedableComponent*> ContainedBreedables;

	/** The genetic mixing logic */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Breeding|Farm")
	TObjectPtr<UPangeaGeneticStrategy> GeneticStrategy;

	/** Default egg class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breeding|Farm")
	TSubclassOf<APangeaEggActor> EggClass;

	/** Fired when an egg is spawned */
	UPROPERTY(BlueprintAssignable, Category = "Breeding|Farm")
	FOnEggSpawned OnEggSpawned;

	/** Returns the breedables currently tracked inside the farm zone. */
	UFUNCTION(BlueprintCallable, Category = "Breeding|Farm")
	TArray<UPangeaBreedableComponent*> GetContainedBreedables() const
	{
		return ContainedBreedables;
	}

	/** Splits the tracked breedables that have a breeding configuration into males and females. */
	UFUNCTION(BlueprintCallable, Category = "Breeding|Farm")
	void GetBreedablesByGender(TArray<UPangeaBreedableComponent*>& OutMales, TArray<UPangeaBreedableComponent*>& OutFemales) const;

	/** Server only. Spawns an egg for a fertile male and female of the same species inside the farm. Returns null when the pair is not valid. */
	UFUNCTION(BlueprintCallable, Category = "Breeding|Farm")
	APangeaEggActor* TryBreed(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female);

	/** Rebuilds the tracked breedables from the actors currently overlapping the farm zone. */
	UFUNCTION(BlueprintCallable, Category = "Breeding|Farm")
	void RefreshContainedBreedables();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	APangeaEggActor* SpawnEgg(UPangeaBreedableComponent* Male,
		UPangeaBreedableComponent* Female,
		UPangeaCreatureDefinition* CreatureDefinition,
		const UPangeaBreedingFragment* BreedingFragment);
	static void ApplyFertilityCooldowns(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female, const UPangeaBreedingFragment* BreedingFragment);
};
