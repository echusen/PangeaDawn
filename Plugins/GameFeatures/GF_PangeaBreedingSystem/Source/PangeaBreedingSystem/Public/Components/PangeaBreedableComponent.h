// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Definitions/PangeaBreedingFragment.h"
#include "GameplayEffectTypes.h"
#include "Interfaces/PDBreedableInterface.h"
#include "Types/BreedingTypes.h"
#include "PangeaBreedableComponent.generated.h"

class APangeaEggActor;
class UAbilitySystemComponent;
class UACFCharacterDataAsset;
class UACFGASStatisticsComponent;
class UPangeaBreedingFarmComponent;
class UPangeaBreedingFragment;
class UPangeaCreatureDefinition;
class UPangeaGeneticStrategy;

/** Broadcast on the server when this creature produced an egg with another parent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBred, APangeaEggActor*, Egg, AActor*, OtherParentActor);

/** Broadcast when the effective fertility (fertile flag and cooldown) of the creature is updated. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFertilityStateChanged, bool, bNowFertile);

/** Broadcast with the seconds left on the fertility cooldown. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFertilityCooldownTick, float, RemainingTime);

/**
 * Makes its owning creature breedable: holds gender, fertility and cooldown, genetic traits, lineage
 * and the inherited stat profile and appearance, all server-authoritative, replicated and saved.
 */
UCLASS(ClassGroup = (Breeding), meta = (BlueprintSpawnableComponent))
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
	virtual FParentSnapshot GetParentSnapshot_Implementation() const override
	{
		return BuildParentSnapshot();
	}

	/** Gender of the creature. Assigned on the server at BeginPlay when left Unspecified. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Breeding")
	ECreatureGender Gender = ECreatureGender::Unspecified;

	/** Explicit fertility flag. The creature can only breed while this is set and no cooldown is running. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_Fertility, Category = "Breeding")
	bool bIsFertile = true;

	/** True while the post-breeding fertility cooldown is running. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_Fertility, Category = "Breeding")
	bool bIsOnFertilityCooldown = false;

	/** Seconds of game time left on the fertility cooldown, as last updated by the server. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_Fertility, Category = "Breeding")
	float FertilityCooldownRemaining = 0.0f;

	FTimerHandle FertilityCooldownTimerHandle;

	/** Server only. Makes the creature infertile for CooldownDuration seconds. Ignored while a cooldown is already running. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breeding")
	void StartFertilityCooldown(float CooldownDuration);

	/** Server only. Ends the fertility cooldown immediately. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breeding")
	void EndFertilityCooldown();

	/** Genetic traits of this creature, passed to its offspring through the parent snapshot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Replicated, Category = "Breeding|Genetics")
	FGeneticTraitSet GeneticTraits;

	/** ACF statistics component of the owner. Looked up on the owner when left unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UACFGASStatisticsComponent> ACFAttributes;

	/** Stat values this creature inherited from its parents. Applied through a gameplay effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Replicated, Category = "Breeding|Genetics")
	FPangeaInheritedStatProfile InheritedStatProfile;

	/** Persistent application data captured at birth; no dependency on the mother's lifetime or juvenile reproduction rules. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Breeding|Genetics")
	TSubclassOf<UGameplayEffect> InheritedStatsApplicationEffect;

	/** Material parameters this creature inherited from its parents. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_InheritedAppearance, Category = "Breeding|Genetics")
	FPangeaInheritedAppearance InheritedAppearance;

	/** Creature id of the first parent, stored at birth. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Replicated, Category = "Breeding|Genetics")
	FGuid ParentAId;

	/** Creature id of the second parent, stored at birth. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Replicated, Category = "Breeding|Genetics")
	FGuid ParentBId;

	/** Zero denotes a record from before breeding state was saved. Missing historical values cannot be reconstructed. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Breeding")
	int32 BreedingStateVersion = 0;

	/** Returns the owner's creature id when the owner provides one, otherwise the id generated by this component. */
	UFUNCTION(BlueprintPure, Category = "Identity")
	FGuid GetCreatureId() const;

	/** Seconds left on the fertility cooldown. Zero when no cooldown is running. */
	UFUNCTION(BlueprintPure, Category = "Breeding")
	float GetFertilityCooldownRemaining() const;

	/** Server only. Stores the traits, gender, appearance and lineage a hatched creature received from its egg. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breeding|Genetics")
	void SetBirthState(
		const FGeneticTraitSet& Traits, ECreatureGender InGender, const FPangeaInheritedAppearance& Appearance, FGuid InParentAId, FGuid InParentBId);

	/** Writes InheritedAppearance to a dynamic material instance on the owner's skeletal mesh. */
	UFUNCTION(BlueprintCallable, Category = "Breeding|Genetics")
	void ApplyInheritedAppearance();

	/** Fired on the server when this creature produced an egg through BreedWith. */
	UPROPERTY(BlueprintAssignable)
	FOnBred OnBred;

	/** Fired when the fertility flag or the cooldown state is updated. */
	UPROPERTY(BlueprintAssignable, Category = "Breeding|Events")
	FFertilityStateChanged OnFertilityStateChanged;

	/** Fired with the seconds left on the fertility cooldown. */
	UPROPERTY(BlueprintAssignable, Category = "Breeding|Events")
	FFertilityCooldownTick OnFertilityCooldownTick;

	FTimerHandle FertilityTickTimerHandle;

	/** Timer callback: refreshes the remaining cooldown and ends the cooldown once it reaches zero. */
	void UpdateFertilityCooldown();

	/** Captures species, identity, traits, inherited stat sources and material parameters of this creature for breeding. */
	UFUNCTION(BlueprintCallable)
	FParentSnapshot BuildParentSnapshot() const;

	/** Server only. Breeds this creature with OtherParent in Farm. Returns the spawned egg, or null when breeding is not possible. */
	UFUNCTION(BlueprintCallable)
	APangeaEggActor* BreedWith(UPangeaBreedableComponent* OtherParent, UPangeaBreedingFarmComponent* Farm);

	/** Reads the species' inheritable color parameters from the owner's skeletal mesh material. */
	UFUNCTION(BlueprintCallable)
	TMap<FName, FLinearColor> CollectMaterialGenetics() const;

	/** Returns the gender of the creature. */
	UFUNCTION(BlueprintCallable, Category = "Breeding")
	ECreatureGender GetGender() const
	{
		return Gender;
	}

	/** Server only. Sets the gender of the creature. */
	UFUNCTION(BlueprintCallable, Category = "Breeding")
	void SetGender(ECreatureGender NewGender);

	/** Server only. Assigns a gender using the creature definition's assignment mode. Keeps an already assigned gender unless bForceReassign is set. */
	UFUNCTION(BlueprintCallable, Category = "Breeding")
	ECreatureGender AssignGenderFromDefinition(bool bForceReassign = false);

	/** True when the creature is female. */
	UFUNCTION(BlueprintPure, Category = "Breeding")
	bool CanProduceEgg() const
	{
		return Gender == ECreatureGender::Female;
	}

	/** Server only. Stores the inherited stat profile and the gameplay effect that applies it, optionally applying it right away. */
	UFUNCTION(BlueprintCallable, Category = "Breeding|Stats")
	void SetInheritedStatProfile(
		const FPangeaInheritedStatProfile& NewProfile, bool bApplyImmediately = true, TSubclassOf<UGameplayEffect> ApplicationEffect = nullptr);

	/** Server only. Applies InheritedStatProfile to the owner's ability system, replacing a previously applied inherited effect. */
	UFUNCTION(BlueprintCallable, Category = "Breeding|Stats")
	void ApplyInheritedStatProfile();

	/** ACF save-system callback: restore the saved modifier after component data is deserialized. */
	UFUNCTION(BlueprintCallable, Category = "Breeding|Stats")
	void OnComponentLoaded();

	/** Server only. Captures the remaining fertility cooldown and stamps the save version; call before the component is serialized. */
	UFUNCTION(BlueprintCallable, Category = "Breeding")
	void OnComponentSaved();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Blueprint hook for copying owner attributes into genetic traits. The native implementation does nothing. */
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
