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
class UACFCharacterDataAsset;

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
    virtual void Serialize(FArchive& Ar) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // IPDBreedableInterface implementation
    virtual bool IsFertile_Implementation() const override;
    virtual bool SetFertile_Implementation(bool bNewFertile) override;
    virtual FParentSnapshot GetParentSnapshot_Implementation() const override { return BuildParentSnapshot(); }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category="Breeding")
    ECreatureGender Gender = ECreatureGender::Unspecified;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing=OnRep_Fertility, Category="Breeding")
    bool bIsFertile = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing=OnRep_Fertility, Category="Breeding")
    bool bIsOnFertilityCooldown = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing=OnRep_Fertility, Category="Breeding")
    float FertilityCooldownRemaining = 0.0f;

    FTimerHandle FertilityCooldownTimerHandle;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Breeding")
    void StartFertilityCooldown(float CooldownDuration);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Breeding")
    void EndFertilityCooldown();
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Replicated, Category="Breeding|Genetics")
    FGeneticTraitSet GeneticTraits;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UACFGASStatisticsComponent> ACFAttributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Replicated, Category="Breeding|Genetics")
    FPangeaInheritedStatProfile InheritedStatProfile;

    /** Persistent application data captured at birth; no dependency on the mother's lifetime or juvenile reproduction rules. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="Breeding|Genetics")
    TSubclassOf<UGameplayEffect> InheritedStatsApplicationEffect;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, ReplicatedUsing=OnRep_InheritedAppearance, Category="Breeding|Genetics")
    FPangeaInheritedAppearance InheritedAppearance;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Replicated, Category="Breeding|Genetics")
    FGuid ParentAId;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Replicated, Category="Breeding|Genetics")
    FGuid ParentBId;

    /** Zero denotes a record from before breeding state was saved. Missing historical values cannot be reconstructed. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category="Breeding")
    int32 BreedingStateVersion = 0;

    UFUNCTION(BlueprintPure, Category="Identity")
    FGuid GetCreatureId() const;

    UFUNCTION(BlueprintPure, Category="Breeding")
    float GetFertilityCooldownRemaining() const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Breeding|Genetics")
    void SetBirthState(const FGeneticTraitSet& Traits, ECreatureGender InGender,
        const FPangeaInheritedAppearance& Appearance, FGuid InParentAId, FGuid InParentBId);

    UFUNCTION(BlueprintCallable, Category="Breeding|Genetics")
    void ApplyInheritedAppearance();

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
    void SetInheritedStatProfile(const FPangeaInheritedStatProfile& NewProfile, bool bApplyImmediately = true,
        TSubclassOf<UGameplayEffect> ApplicationEffect = nullptr);

    UFUNCTION(BlueprintCallable, Category="Breeding|Stats")
    void ApplyInheritedStatProfile();

    /** ACF save-system callback: restore the saved modifier after component data is deserialized. */
    UFUNCTION(BlueprintCallable, Category="Breeding|Stats")
    void OnComponentLoaded();

    UFUNCTION(BlueprintCallable, Category="Breeding")
    void OnComponentSaved();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintNativeEvent)
    void PullAttributesIntoTraits(FGeneticTraitSet& InOutTraits) const;

private:
    /** Only used on actors without actor-owned creature identity. Pangea dinosaurs keep identity outside the feature. */
    UPROPERTY(SaveGame, Replicated)
    FGuid FallbackCreatureId;

    UFUNCTION()
    void OnRep_Fertility();
    UFUNCTION()
    void OnRep_InheritedAppearance();
    UFUNCTION()
    void HandleAppearanceChanged(UACFCharacterDataAsset* Profile);
    void EnsureFallbackIdentity();
    void ResumeFertilityCooldown();
    const UPangeaBreedingFragment* GetBreedingFragment() const;
    UPangeaCreatureDefinition* GetCreatureDefinition() const;
    UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
    float GetInheritedSourceValue(EPangeaInheritedStatType StatType, const FGameplayTag& StatTag) const;
    static ECreatureGender GetRandomGender();

    FActiveGameplayEffectHandle InheritedStatsEffectHandle;
};
