// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TamingTypes.h"
#include "PangeaTamingComponent.generated.h"

class UTamingWidget;
class UTameSpeciesConfig;
class UGameplayAbility;
class UGameplayEffect;
class AAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameStateChanged, ETameState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameRoleSelected, ETamedRole, Role);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PANGEATAMINGSYSTEM_API UPangeaTamingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPangeaTamingComponent();

	// --- Species data ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config", SaveGame)
	UTameSpeciesConfig* TameSpeciesConfig = nullptr;

	// --- Runtime state ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State", SaveGame)
	ETameState TamedState = ETameState::Wild;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State", SaveGame)
	ETamedRole TamedRole = ETamedRole::None;

	// // Persisted current team for SaveGame loading
	// UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State", SaveGame)
	// FGameplayTag ChangedTeam;
	//
	// // Persisted AI controller class for SaveGame loading
	// UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State", SaveGame)
	// TSubclassOf<AAIController> CurrentAIControllerClass;

	// --- Core API ---
	UFUNCTION(BlueprintCallable, Category="Taming")
	void InitializeWild();

	UFUNCTION(BlueprintCallable, Category="Taming")
	void InitializeHostile();

	UFUNCTION(BlueprintCallable, Category="Taming")
	void StartTameAttempt(AActor* Instigator);

	UFUNCTION(BlueprintCallable, Category="Taming")
	void OnTameResolved(bool bSuccess, ETamedRole DesiredRole);

	UFUNCTION(BlueprintCallable, Category="Taming")
	void HandleTameStateChanged(ETameState NewState);

	UFUNCTION(BlueprintCallable, Category="Load")
	void HandleLoadedActor();

	UFUNCTION(BlueprintCallable, Category="Taming")
	void SetTamedRole(ETamedRole NewRole);

	UFUNCTION(BlueprintCallable, Category="Taming")
	ETamedRole GetTamedRole() const {return TamedRole;}

	UFUNCTION(BlueprintCallable, Category="Taming")
	ETameState GetTameState() const {return TamedState;}

	// --- Minigame ---
	UPROPERTY(EditDefaultsOnly, Category="Taming|UI")
	TSubclassOf<UTamingWidget> MinigameWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveMinigameWidget;

	UFUNCTION(BlueprintCallable, Category="Taming|Minigame")
	void StartMinigame(AActor* Instigator, AActor* Target, float Duration);

	UFUNCTION(BlueprintCallable, Category="Taming|Minigame")
	void OnMinigameResult(bool bSuccess);

	// --- Events ---
	UPROPERTY(BlueprintAssignable)
	FTameStateChanged OnTameStateChanged;

	UPROPERTY(BlueprintAssignable)
	FTameRoleSelected OnTameRoleSelected;

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedInstigator;

protected:
	virtual void BeginPlay() override;

private:
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
		if (!Actor || !Tag.IsValid()) return;
		FGameplayTagContainer Tags; Tags.AddTag(Tag);
		UAbilitySystemBlueprintLibrary::AddLooseGameplayTags(Actor, Tags);
	}

	inline void RemoveTagFromActor(AActor* Actor, const FGameplayTag& Tag)
	{
		if (!Actor || !Tag.IsValid()) return;
		FGameplayTagContainer Tags; Tags.AddTag(Tag);
		UAbilitySystemBlueprintLibrary::RemoveLooseGameplayTags(Actor, Tags);
	}
};
