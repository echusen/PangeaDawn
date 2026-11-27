// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Definitions/PangeaBreedingFragment.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Items/ACFWorldItem.h"
#include "Types/BreedingTypes.h"
#include "PangeaEggActor.generated.h"

class UPangeaGeneticStrategy;

/** Lifecycle of an egg, from spawn to the moment its creature has hatched. */
UENUM(BlueprintType)
enum class EPangeaEggState : uint8
{
	/** Spawned but InitializeEgg has not succeeded yet. */
	Uninitialized,
	/** Offspring outcome is generated and the incubation timer is counting down. */
	Incubating,
	/** Incubation finished; the egg hatches on the next attempt. */
	ReadyToHatch,
	/** The creature was spawned from this egg. */
	Hatched
};

/** Broadcast with the incubation progress (0 to 1) whenever the incubation state updates. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggProgress, float, Progress);

/** Broadcast on the server with the creature that hatched from the egg. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggHatched, AActor*, NewCreature);

/** Server-owned, saveable incubation and generated-once offspring state. Timers use game time only. */
UCLASS()
class PANGEABREEDINGSYSTEM_API APangeaEggActor : public AACFWorldItem
{
	GENERATED_BODY()
public:
	APangeaEggActor();
	virtual void Serialize(FArchive& Ar) override;

	/** Definition of the parents' species; source of the breeding configuration used by this egg. */
	UPROPERTY(ReplicatedUsing = OnRep_CreatureDefinition, SaveGame)
	TObjectPtr<UPangeaCreatureDefinition> CreatureDefinition;

	/** Snapshot of the first parent taken when the egg was initialized. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category = "Breeding")
	FParentSnapshot ParentA;

	/** Snapshot of the second parent taken when the egg was initialized. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category = "Breeding")
	FParentSnapshot ParentB;

	/** Genetic traits of the offspring, combined once from both parents by the genetic strategy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category = "Breeding")
	FGeneticTraitSet ChildTraits;

	/** Current lifecycle state of the egg. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_Incubation, SaveGame, Category = "Breeding")
	EPangeaEggState EggState = EPangeaEggState::Uninitialized;

	/** Gender the offspring receives when it hatches, decided when the egg is initialized. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, SaveGame, Category = "Breeding")
	ECreatureGender ChildGender = ECreatureGender::Unspecified;

	/** Inherited stat values the offspring receives when it hatches. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, SaveGame, Category = "Breeding")
	FPangeaInheritedStatProfile ChildStatProfile;

	/** Inherited material parameters the offspring receives when it hatches. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, SaveGame, Category = "Breeding")
	FPangeaInheritedAppearance ChildAppearance;

	/** Definition of the creature that hatches from this egg. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Breeding")
	TObjectPtr<UPangeaCreatureDefinition> OffspringDefinition;

	/** Gameplay effect handed to the offspring to apply ChildStatProfile. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Breeding")
	TSubclassOf<UGameplayEffect> ChildStatsApplicationEffect;

	/** Save format version of the egg state. Zero denotes an uninitialized egg or a record saved before versioning. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category = "Breeding")
	int32 EggStateVersion = 0;

	/** Returns the definition of the parents' species. */
	UFUNCTION(BlueprintPure, Category = "Breeding")
	UPangeaCreatureDefinition* GetCreatureDefinition() const
	{
		return CreatureDefinition;
	}

	/** Returns the species id of the parents' definition, or None when no definition is set. */
	UFUNCTION(BlueprintPure, Category = "Breeding")
	FName GetSpeciesID() const
	{
		return CreatureDefinition != nullptr ? CreatureDefinition->SpeciesId : NAME_None;
	}

	/** True once InitializeEgg has succeeded. */
	UFUNCTION(BlueprintPure, Category = "Breeding")
	bool IsInitialized() const
	{
		return EggState != EPangeaEggState::Uninitialized;
	}

	/** Seconds of incubation left. Zero unless the egg is incubating. */
	UFUNCTION(BlueprintPure, Category = "Breeding")
	float GetIncubationRemaining() const;

	/** Incubation progress from 0 (just laid) to 1 (ready to hatch). */
	UFUNCTION(BlueprintPure, Category = "Breeding")
	float GetIncubationProgress() const;

	/** Fired with the incubation progress whenever the incubation state updates. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnEggProgress OnEggProgress;

	/** Fired on the server after the creature has hatched, just before the egg is destroyed. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnEggHatched OnEggHatched;

	/** Server only. Generates the offspring outcome once from both parent snapshots and starts incubation. Ignored when already initialized or when the parents do not match the definition's species. */
	void InitializeEgg(
		const FParentSnapshot& InA, const FParentSnapshot& InB, UPangeaGeneticStrategy* Strategy, UPangeaCreatureDefinition* InCreatureDefinition);

	/** Explicit hatch command (may be used to force early hatching). Failed attempts preserve the cached outcome. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breeding")
	AActor* Hatch();

	/** Applies the cached ChildAppearance to the first skeletal mesh of NewCreature. */
	void ApplyVisualInheritance(AActor* NewCreature);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnSaved_Implementation() override;
	virtual void OnLoaded_Implementation() override;
	virtual bool ShouldBeIgnored_Implementation() override;
	virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_CreatureDefinition();

	UFUNCTION()
	void OnRep_Incubation();

	/** Durable durations, never timestamps from a prior world/session or wall clock. */
	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Incubation)
	float IncubationRemaining = 0.f;

	/** Full incubation duration in seconds, fixed when the egg is initialized. */
	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Incubation)
	float TotalIncubationTime = 0.f;

private:
	FTimerHandle HatchTimerHandle;
	FTimerHandle IncubationTickHandle;

	bool bHatchInProgress = false;

	void StartIncubation();
	void ClearIncubationTimers();
	void TickIncubation();
	void FinishIncubation();

	FPangeaInheritedStatProfile GenerateInheritedStatProfile(const UPangeaBreedingFragment* BreedingFragment) const;
	FPangeaInheritedAppearance GenerateInheritedAppearance(const UPangeaBreedingFragment* BreedingFragment) const;

	float GetSnapshotInheritedValue(const FParentSnapshot& Snapshot, EPangeaInheritedStatType StatType, const FGameplayTag& StatTag) const;

	FLinearColor MixParentColors(const FLinearColor& ColorA, const FLinearColor& ColorB) const;
	float MixParentScalars(float ValueA, float ValueB) const;
};
