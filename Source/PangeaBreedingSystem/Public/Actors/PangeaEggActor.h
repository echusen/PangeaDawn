// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/PangeaBreedingSystemTypes.h"
#include "Data/PangeaSpeciesDataAsset.h"
#include "Items/ACFWorldItem.h"
#include "PangeaEggActor.generated.h"

class UPangeaGeneticStrategy;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggProgress, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggHatched, AActor*, NewCreature);

/**
 * 
 */
UCLASS()
class PANGEABREEDINGSYSTEM_API APangeaEggActor : public AACFWorldItem
{
	GENERATED_BODY()
    
public:
    APangeaEggActor();
    
    UPROPERTY(ReplicatedUsing=OnRep_Species, Transient)
    TObjectPtr<UPangeaSpeciesDataAsset> SpeciesData;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Breeding")
    FParentSnapshot ParentA;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Breeding")
    FParentSnapshot ParentB;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Breeding")
    FGeneticTraitSet ChildTraits;

    UFUNCTION(BlueprintCallable, Category="Breeding")
    UPangeaSpeciesDataAsset* GetSpeciesData() const { return SpeciesData; }

    // (optional convenience)
    UFUNCTION(BlueprintCallable, Category="Breeding")
    FName GetSpeciesID() const { return SpeciesData->SpeciesID; }

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEggProgress OnEggProgress;

    UPROPERTY(BlueprintAssignable)
    FOnEggHatched OnEggHatched;

    void InitializeEgg(const FParentSnapshot& InA, const FParentSnapshot& InB, UPangeaGeneticStrategy* Strategy, UPangeaSpeciesDataAsset* InSpecies);

    UFUNCTION(BlueprintCallable)
    AActor* Hatch();
        void ApplyVisualInheritance(AActor* NewCreature);

        virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void OnSaved_Implementation() override;
    virtual bool ShouldBeIgnored_Implementation() override;
    virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnRep_Species();

    FTimerHandle HatchTimerHandle;
    FTimerHandle IncubationTickHandle;

    float CurrentIncubationTime = 0.0f;
    float TotalIncubationTime = 0.0f;

    void StartIncubation();
    void TickIncubation();
    void FinishIncubation();

    bool ValidateVisualInheritance(AActor* NewCreature) const;
    UMaterialInstanceDynamic* CreateDynamicMaterial(AActor* NewCreature, int32 SlotIndex);
    FLinearColor MixParentColors(const FLinearColor& ColorA, const FLinearColor& ColorB) const;
    void ApplyMaterialGroupInheritance(UMaterialInstanceDynamic* MID, const FMaterialGeneticGroup& Group) const;
};
