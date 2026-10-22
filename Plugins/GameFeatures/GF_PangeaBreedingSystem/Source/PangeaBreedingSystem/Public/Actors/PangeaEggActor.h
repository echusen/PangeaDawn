// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Definitions/PangeaBreedingFragment.h"
#include "Definitions/PangeaCreatureDefinition.h"
#include "Types/BreedingTypes.h"
#include "Items/ACFWorldItem.h"
#include "PangeaEggActor.generated.h"

class UPangeaGeneticStrategy;

UENUM(BlueprintType)
enum class EPangeaEggState : uint8
{
    Uninitialized,
    Incubating,
    ReadyToHatch,
    Hatched
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggProgress, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEggHatched, AActor*, NewCreature);

/** Server-owned, saveable incubation and generated-once offspring state. Timers use game time only. */
UCLASS()
class PANGEABREEDINGSYSTEM_API APangeaEggActor : public AACFWorldItem
{
    GENERATED_BODY()
public:
    APangeaEggActor();
    virtual void Serialize(FArchive& Ar) override;

    UPROPERTY(ReplicatedUsing=OnRep_CreatureDefinition, SaveGame)
    TObjectPtr<UPangeaCreatureDefinition> CreatureDefinition;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category="Breeding")
    FParentSnapshot ParentA;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category="Breeding")
    FParentSnapshot ParentB;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category="Breeding")
    FGeneticTraitSet ChildTraits;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Incubation, SaveGame, Category="Breeding")
    EPangeaEggState EggState = EPangeaEggState::Uninitialized;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, SaveGame, Category="Breeding")
    ECreatureGender ChildGender = ECreatureGender::Unspecified;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, SaveGame, Category="Breeding")
    FPangeaInheritedStatProfile ChildStatProfile;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, SaveGame, Category="Breeding")
    FPangeaInheritedAppearance ChildAppearance;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category="Breeding")
    TObjectPtr<UPangeaCreatureDefinition> OffspringDefinition;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category="Breeding")
    TSubclassOf<UGameplayEffect> ChildStatsApplicationEffect;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, SaveGame, Category="Breeding")
    int32 EggStateVersion = 0;

    UFUNCTION(BlueprintPure, Category="Breeding")
    UPangeaCreatureDefinition* GetCreatureDefinition() const { return CreatureDefinition; }

    UFUNCTION(BlueprintPure, Category="Breeding")
    FName GetSpeciesID() const { return CreatureDefinition ? CreatureDefinition->SpeciesId : NAME_None; }

    UFUNCTION(BlueprintPure, Category="Breeding")
    bool IsInitialized() const { return EggState != EPangeaEggState::Uninitialized; }

    UFUNCTION(BlueprintPure, Category="Breeding")
    float GetIncubationRemaining() const;

    UFUNCTION(BlueprintPure, Category="Breeding")
    float GetIncubationProgress() const;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnEggProgress OnEggProgress;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnEggHatched OnEggHatched;

    void InitializeEgg(const FParentSnapshot& InA, const FParentSnapshot& InB, UPangeaGeneticStrategy* Strategy, UPangeaCreatureDefinition* InCreatureDefinition);

    /** Explicit hatch command (may be used to force early hatching). Failed attempts preserve the cached outcome. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Breeding")
    AActor* Hatch();

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
    UPROPERTY(SaveGame, ReplicatedUsing=OnRep_Incubation)
    float IncubationRemaining = 0.f;

    UPROPERTY(SaveGame, ReplicatedUsing=OnRep_Incubation)
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
