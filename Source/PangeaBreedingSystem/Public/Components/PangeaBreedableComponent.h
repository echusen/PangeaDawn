// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PangeaBreedingSystemTypes.h"
#include "Interfaces/PangeaBreedingInterface.h"
#include "PangeaBreedableComponent.generated.h"

class UPangeaGeneticStrategy;
class UPangeaSpeciesDataAsset;
class UARSStatisticsComponent;
class UPangeaBreedingFarmComponent;

UENUM(BlueprintType)
enum class ECreatureGender : uint8
{
    Male UMETA(DisplayName = "Male"),
    Female UMETA(DisplayName = "Female")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBred, class APangeaEggActor*, Egg, AActor*, OtherParentActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFertilityStateChanged, bool, bNowFertile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFertilityCooldownTick, float, RemainingTime);

UCLASS( ClassGroup=(Breeding), meta=(BlueprintSpawnableComponent) )
class PANGEABREEDINGSYSTEM_API UPangeaBreedableComponent : public UActorComponent, public IPangeaBreedingInterface
{
	GENERATED_BODY()

public:	
    // Sets default values for this component's properties
    UPangeaBreedableComponent();

    // IPangeaBreedingInterface implementation
    virtual bool IsFertile_Implementation() const override;
    virtual bool SetFertile_Implementation(bool bNewFertile) override;
    virtual FParentSnapshot GetParentSnapshot_Implementation() const override { return BuildParentSnapshot(); }
    virtual UPangeaSpeciesDataAsset* GetSpeciesData_Implementation() const override { return SpeciesData; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Breeding")
    ECreatureGender Gender = ECreatureGender::Male;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsFertile = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Breeding")
    bool bIsOnFertilityCooldown = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Breeding")
    float FertilityCooldownRemaining = 0.0f;

    FTimerHandle FertilityCooldownTimerHandle;

    void StartFertilityCooldown(float CooldownDuration);
    void EndFertilityCooldown();
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGeneticTraitSet GeneticTraits;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UARSStatisticsComponent> ACFAttributes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Breeding|Setup")
    TObjectPtr<UPangeaSpeciesDataAsset> SpeciesData;

    UPROPERTY(BlueprintAssignable)
    FOnBred OnBred;

    UPROPERTY(BlueprintAssignable, Category = "Breeding|Events")
    FFertilityStateChanged OnFertilityStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Breeding|Events")
    FFertilityCooldownTick OnFertilityCooldownTick;

    FTimerHandle FertilityTickTimerHandle;

    void UpdateFertilityCooldown();
    
    UFUNCTION(BlueprintCallable)
    FParentSnapshot BuildParentSnapshot() const;

    UFUNCTION(BlueprintCallable)
    class APangeaEggActor* BreedWith(UPangeaBreedableComponent* OtherParent, UPangeaBreedingFarmComponent* Farm);

    UFUNCTION(BlueprintCallable)
    TMap<FName, FLinearColor> CollectMaterialGenetics() const;

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintNativeEvent)
    void PullAttributesIntoTraits(FGeneticTraitSet& InOutTraits) const;
};
