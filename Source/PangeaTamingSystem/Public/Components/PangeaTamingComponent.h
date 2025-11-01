// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TamingTypes.h"
#include "PangeaTamingComponent.generated.h"

class UTamingWidget;
class APDDinosaurBase;
class UTameSpeciesConfig;
class UGameplayAbility;
class UGameplayEffect;
class AAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameStateChanged, ETameState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameRoleSelected, ETamedRole, Role);



UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PANGEATAMINGSYSTEM_API UPangeaTamingComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPangeaTamingComponent();

	// --- Species data ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	UTameSpeciesConfig* SpeciesConfig = nullptr;

	// --- Runtime state ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State")
	ETameState TameState = ETameState::Wild;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State")
	ETamedRole TamedRole = ETamedRole::None;

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
	void HandleTameFailed(const FString& Reason, AActor* Instigator);

	UFUNCTION(BlueprintCallable, Category="Taming")
	void SetTamedRole(ETamedRole NewRole);

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

protected:
	virtual void BeginPlay() override;

private:
	bool HasRequiredStats(AActor* Instigator) const;
	bool HasRequiredItem(AActor* Instigator) const;

	void ClearStateTags();
	void ClearRoleTags();
	void ApplyStateTags();
	void ApplyRoleTag(ETamedRole Role);
	void ChangeTeam(const FGameplayTag& TeamTag) const;
	void GrantTamedAbilities() const;
	void SwitchAIController(TSubclassOf<AAIController> NewControllerClass) const;

	// Tag helpers
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

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedInstigator;
};
