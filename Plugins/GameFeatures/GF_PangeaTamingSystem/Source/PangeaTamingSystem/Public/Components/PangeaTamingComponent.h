// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/PDTameableInterface.h"
#include "Types/TamingTypes.h"
#include "PangeaTamingComponent.generated.h"

class AAIController;
class UGameplayAbility;
class UGameplayEffect;
class UTameSpeciesConfig;
class UTamingWidget;
class UUserWidget;

/** Broadcast when the creature's tame state changes or is re-applied at startup or after loading. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameStateChanged, ETameState, NewState);

/** Broadcast when the creature's tamed role is set. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameRoleSelected, ETamedRole, Role);

/**
 * Makes its owner tameable. Owns the wild / hostile / tamed state and the tamed role of a creature,
 * checks the tamer's item and stat prerequisites, runs the taming minigame and applies the
 * resulting gameplay tags, team, AI controller, abilities and effects from a UTameSpeciesConfig.
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PANGEATAMINGSYSTEM_API UPangeaTamingComponent : public UActorComponent, public IPDTameableInterface
{
	GENERATED_BODY()

public:
	UPangeaTamingComponent();

	// --- Species data ---

	/** Taming rules for this creature. When unset it is resolved at BeginPlay from the owner's creature definition, then from OwnerClassConfigMap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config", SaveGame)
	TObjectPtr<UTameSpeciesConfig> TameSpeciesConfig = nullptr;

	// Optional per-owner-class overrides, useful when this component is injected into many dinosaur subclasses.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TMap<TSoftClassPtr<AActor>, TObjectPtr<UTameSpeciesConfig>> OwnerClassConfigMap;

	// --- Runtime state ---

	/** Current tame state of the owner. Saved with the game. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State", SaveGame)
	ETameState TamedState = ETameState::Wild;

	/** Role the owner fulfils once tamed. Saved with the game. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State", SaveGame)
	ETamedRole TamedRole = ETamedRole::None;

	// --- Core API ---

	/** Puts the owner in the Wild state: applies the wild state tag and team, then broadcasts OnTameStateChanged. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	void InitializeWild();

	/** Puts the owner in the Hostile state: applies the hostile state tag and team, then broadcasts OnTameStateChanged. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	void InitializeHostile();

	/** Checks the prerequisites for Instigator and, when they pass, triggers the configured tame action on it. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	virtual void StartTameAttempt(AActor* Instigator) override;

	/** Finishes a tame attempt. Success also requires the cached instigator to still hold the required item. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	void OnTameResolved(bool bSuccess, ETamedRole DesiredRole);

	/** Reacts to a tame state change: adds a tamed companion to the tamer's group and refreshes the tamer's interactions. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	void HandleTameStateChanged(ETameState NewState);

	/** Re-applies tags, team, role and abilities after a save is loaded, without consuming items or success effects again. */
	UFUNCTION(BlueprintCallable, Category = "Load")
	virtual void HandleLoadedActor() override;

	/** Sets the tamed role, falling back to a role the species allows, and switches to that role's AI controller. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	void SetTamedRole(ETamedRole NewRole);

	/** Returns the role the owner fulfils once tamed. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	virtual ETamedRole GetTamedRole() const override
	{
		return TamedRole;
	}

	/** Returns the current tame state of the owner. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	virtual ETameState GetTameState() const override
	{
		return TamedState;
	}

	/** True when a species config is set and it allows at least one tamed role. */
	UFUNCTION(BlueprintCallable, Category = "Taming")
	virtual bool CanBeTamed() const override;

	// --- Minigame ---

	/** Taming minigame widget class. Not read by native code: the minigame page is spawned from the UI.Widget.Pangea.Taming tag. */
	UPROPERTY(EditDefaultsOnly, Category = "Taming|UI")
	TSubclassOf<UTamingWidget> MinigameWidgetClass;

	/** Minigame widget currently on screen, or null when no minigame is running. */
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveMinigameWidget;

	/** Opens the taming minigame for the local player controlling Instigator. Target must be this component's owner. */
	UFUNCTION(BlueprintCallable, Category = "Taming|Minigame")
	void StartMinigame(AActor* Instigator, AActor* Target, float Duration);

	/** Closes the minigame widget and resolves the tame attempt with its result. */
	UFUNCTION(BlueprintCallable, Category = "Taming|Minigame")
	void OnMinigameResult(bool bSuccess);

	// --- Events ---

	/** Broadcast when the tame state changes or is re-applied at startup or after loading. */
	UPROPERTY(BlueprintAssignable)
	FTameStateChanged OnTameStateChanged;

	/** Broadcast when the tamed role is set. */
	UPROPERTY(BlueprintAssignable)
	FTameRoleSelected OnTameRoleSelected;

	/** Actor that started the current or most recent tame attempt. */
	UPROPERTY()
	TWeakObjectPtr<AActor> CachedInstigator;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ResolveConfigFromDefinition();
	void ResolveConfigFromOwnerClass();
	void InitializeStartupTameState();

	// --- Checks ---
	bool HasRequiredStats(AActor* Instigator) const;
	bool HasRequiredItem(AActor* Instigator) const;
	bool CheckTamePrerequisites(AActor* Instigator) const;

	// --- Transitions ---
	void TransitionToTamedState(ETamedRole DesiredRole);
	void TransitionToFailedState(AActor* Instigator);
	void StartTameCooldown(AActor* Instigator);

	// --- Effects & Items ---
	void ApplyTameSuccessEffects() const;
	void ConsumeTameItems() const;

	// --- Tags & State ---
	void ClearStateTags();
	void ClearRoleTags();
	void ClearTameTags();
	void ApplyStateTags();
	void ApplyRoleTag(ETamedRole Role);

	// --- Utility ---
	void ChangeTeam(const FGameplayTag& TeamTag);
	void GrantTamedAbilities() const;
	void SwitchAIController(TSubclassOf<AAIController> NewControllerClass);
	ETamedRole DetermineFinalRole(ETamedRole DesiredRole) const;

	// --- Tag helpers ---
	inline void AddTagToActor(AActor* Actor, const FGameplayTag& Tag)
	{
		if (Actor == nullptr || !Tag.IsValid())
		{
			return;
		}

		FGameplayTagContainer Tags;
		Tags.AddTag(Tag);
		UAbilitySystemBlueprintLibrary::AddLooseGameplayTags(Actor, Tags);
	}

	inline void RemoveTagFromActor(AActor* Actor, const FGameplayTag& Tag)
	{
		if (Actor == nullptr || !Tag.IsValid())
		{
			return;
		}

		FGameplayTagContainer Tags;
		Tags.AddTag(Tag);
		UAbilitySystemBlueprintLibrary::RemoveLooseGameplayTags(Actor, Tags);
	}
};
