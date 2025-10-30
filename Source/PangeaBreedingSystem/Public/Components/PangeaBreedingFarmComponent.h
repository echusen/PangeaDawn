// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/PangeaFarmInterface.h"
#include "PangeaBreedingFarmComponent.generated.h"

class UBoxComponent;
class UPangeaBreedableComponent;
class APangeaEggActor;
class UPangeaGeneticStrategy;
class UPangeaSpeciesDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggSpawned, APangeaEggActor*, Egg);

UCLASS( ClassGroup=(Breeding), meta=(BlueprintSpawnableComponent) )
class PANGEABREEDINGSYSTEM_API UPangeaBreedingFarmComponent : public UActorComponent, public IPangeaFarmInterface
{
	GENERATED_BODY()

public:	
    // Sets default values for this component's properties
    UPangeaBreedingFarmComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding|References")
    TObjectPtr<UBoxComponent> BreedingZone;

    /** List of breedables currently inside the farm zone */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Breeding|Farm")
    TArray<UPangeaBreedableComponent*> ContainedBreedables;

    /** The genetic mixing logic */
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category="Breeding|Farm")
    TObjectPtr<UPangeaGeneticStrategy> GeneticStrategy;

    /** Default egg class */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding|Farm")
    TSubclassOf<APangeaEggActor> EggClass;

    /** Fired when an egg is spawned */
    UPROPERTY(BlueprintAssignable, Category="Breeding|Farm")
    FOnEggSpawned OnEggSpawned;

    UFUNCTION(BlueprintCallable, Category="Breeding|Farm")
    TArray<UPangeaBreedableComponent*> GetContainedBreedables() const { return ContainedBreedables; }

    UFUNCTION(BlueprintCallable, Category="Breeding|Farm")
    void GetBreedablesByGender(TArray<UPangeaBreedableComponent*>& OutMales, TArray<UPangeaBreedableComponent*>& OutFemales) const;

    UFUNCTION(BlueprintCallable, Category="Breeding|Farm")
    APangeaEggActor* TryBreed(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female);

    UFUNCTION(BlueprintCallable, Category="Breeding|Farm")
    void RefreshContainedBreedables();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
    APangeaEggActor* SpawnEgg(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female, UPangeaSpeciesDataAsset* SpeciesDataAsset);
    static void ApplyFertilityCooldowns(UPangeaBreedableComponent* Male, UPangeaBreedableComponent* Female, const UPangeaSpeciesDataAsset* SpeciesDataAsset);
};
