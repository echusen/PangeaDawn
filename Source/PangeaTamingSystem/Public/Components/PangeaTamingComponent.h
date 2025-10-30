// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TamingTypes.h"
#include "PangeaTamingComponent.generated.h"

class APDDinosaurBase;
class UTameSpeciesConfig;
class UGameplayAbility;
class UGameplayEffect;
class AAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameStateChanged, ETameState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameRoleSelected, ETamedRole, Role);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTameFailedDelegate, const FString&, Reason);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PANGEATAMINGSYSTEM_API UPangeaTamingComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPangeaTamingComponent();

	// Species data
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
	UTameSpeciesConfig* SpeciesConfig = nullptr;
	
	// Runtime state
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State")
	ETameState TameState = ETameState::Wild;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="State")
	ETamedRole TamedRole = ETamedRole::None;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Taming")
	FORCEINLINE ETameState GetTameState() const { return TameState; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Taming")
	FORCEINLINE ETamedRole GetTamedRole() const { return TamedRole; }

	UFUNCTION(BlueprintCallable, Category="Taming")
	void SetTameState(ETameState NewState) { TameState = NewState; }


	// API
	UFUNCTION(BlueprintCallable, Category="Taming")
	void InitializeWild();
	
	UFUNCTION(BlueprintCallable, Category="Taming")
	void InitializeHostile();
	
	UFUNCTION(BlueprintCallable, Category="Taming")
	void StartTameAttempt(AActor* Instigator);
	
	UFUNCTION(BlueprintCallable, Category="Taming")
	void AbortTameAttempt();
	
	/** External callback from ACFU action or ability */
	UFUNCTION(BlueprintCallable, Category="Taming")
	void OnTameResolved(bool bSuccess, ETamedRole DesiredRole);

	UPROPERTY(BlueprintAssignable, Category="Taming")
	FTameFailedDelegate OnTameFailed;

	UFUNCTION(BlueprintCallable, Category="Taming")
	void HandleTameFailed(const FString& Reason);
	
	/** Explicit role set post-success if you need to switch later */
	UFUNCTION(BlueprintCallable, Category="Taming")
	void SetTamedRole(ETamedRole NewRole);
	
	// Events
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

	inline void AddTagToActor(AActor* Actor, const FGameplayTag& GameplayTagToAdd)
	{
		if (!Actor || !GameplayTagToAdd.IsValid()) return;
		
		FGameplayTagContainer Tags;
		Tags.AddTag(GameplayTagToAdd);
		UAbilitySystemBlueprintLibrary::AddLooseGameplayTags(Actor, Tags);
	}

	inline void RemoveTagFromActor(AActor* Actor, const FGameplayTag& GameplayTagToRemove)
	{
		if (!Actor || !GameplayTagToRemove.IsValid()) return;
		
		FGameplayTagContainer Tags;
		Tags.AddTag(GameplayTagToRemove);
		UAbilitySystemBlueprintLibrary::RemoveLooseGameplayTags(Actor, Tags);
	}
};
