// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Definitions/PangeaBreedingFragment.h"
#include "GameplayEffectTypes.h"
#include "Interfaces/PDBreedableInterface.h"
#include "Types/BreedingTypes.h"
#include "PangeaBreedableComponent.generated.h"

class UACFGASStatisticsComponent;
class UPangeaBreedingFragment;
class UPangeaGeneticStrategy;
class UPangeaCreatureDefinition;
class UAbilitySystemComponent;
class UPangeaBreedingFarmComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBred, class APangeaEggActor*, Egg, AActor*, OtherParentActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFertilityStateChanged, bool, bNowFertile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFertilityCooldownTick, float, RemainingTime);

UCLASS( ClassGroup=(Breeding), meta=(BlueprintSpawnableComponent) )
class PANGEABREEDINGSYSTEM_API UPangeaBreedableComponent : public UActorComponent, public IPDBreedableInterface
{
	GENERATED_BODY()

public:	
    // Sets default values for this component's properties
    UPangeaBreedableComponent();

    // IPDBreedableInterface implementation
    virtual bool IsFertile_Implementation() const override;
    virtual bool SetFertile_Implementation(bool bNewFertile) override;
    virtual FParentSnapshot GetParentSnapshot_Implementation() const override { return BuildParentSnapshot(); }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Breeding")
    ECreatureGender Gender = ECreatureGender::Unspecified;

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
    TObjectPtr<UACFGASStatisticsComponent> ACFAttributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Breeding|Genetics")
    FPangeaInheritedStatProfile InheritedStatProfile;

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

    UFUNCTION(BlueprintCallable, Category="Breeding")
    ECreatureGender GetGender() const { return Gender; }

    UFUNCTION(BlueprintCallable, Category="Breeding")
    void SetGender(ECreatureGender NewGender);

    UFUNCTION(BlueprintCallable, Category="Breeding")
    ECreatureGender AssignGenderFromDefinition(bool bForceReassign = false);

    UFUNCTION(BlueprintPure, Category="Breeding")
    bool CanProduceEgg() const { return Gender == ECreatureGender::Female; }

    UFUNCTION(BlueprintCallable, Category="Breeding|Stats")
    void SetInheritedStatProfile(const FPangeaInheritedStatProfile& NewProfile, bool bApplyImmediately = true);

    UFUNCTION(BlueprintCallable, Category="Breeding|Stats")
    void ApplyInheritedStatProfile();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintNativeEvent)
    void PullAttributesIntoTraits(FGeneticTraitSet& InOutTraits) const;

private:
    const UPangeaBreedingFragment* GetBreedingFragment() const;
    UPangeaCreatureDefinition* GetCreatureDefinition() const;
    UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
    float GetInheritedSourceValue(EPangeaInheritedStatType StatType, const FGameplayTag& StatTag) const;
    static ECreatureGender GetRandomGender();

    FActiveGameplayEffectHandle InheritedStatsEffectHandle;
};
