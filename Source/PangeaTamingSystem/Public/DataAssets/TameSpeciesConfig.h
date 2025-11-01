// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Items/ACFItem.h"
#include "TameSpeciesConfig.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class AAIController;
class UCurveFloat;

USTRUCT(BlueprintType)
struct FTameStatRequirement
{
	GENERATED_BODY()

	// Attribute to read (e.g., Attributes.Dexterity from your AttributeSet)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Taming|Requirements")
	FGameplayAttribute Attribute;

	// Optional label tag for designers/UI
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Taming|Requirements")
	FGameplayTag DisplayTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Taming|Requirements")
	float MinValue = 0.f;
};

/**
 * 
 */
UCLASS(BlueprintType)
class PANGEATAMINGSYSTEM_API UTameSpeciesConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Roles allowed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
	bool bCanBeMount = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
	bool bCanBeCompanion = true;

	// State tags (authoritative state flags for BT/logic)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|State")
	FGameplayTag WildStateTag = FGameplayTag::RequestGameplayTag("Tame.State.Wild");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|State")
	FGameplayTag HostileStateTag = FGameplayTag::RequestGameplayTag("Tame.State.Hostile");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|State")
	FGameplayTag TamedStateTag = FGameplayTag::RequestGameplayTag("Tame.State.Tamed");

	// Team tags (replace GenericTeamId)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Team")
	FGameplayTag WildTeamTag = FGameplayTag::RequestGameplayTag("Teams.Neutral");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Team")
	FGameplayTag HostileTeamTag = FGameplayTag::RequestGameplayTag("Teams.Enemies");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Team")
	FGameplayTag TamedTeamTag = FGameplayTag::RequestGameplayTag("Teams.Heroes");

	// Role tags
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Role")
	FGameplayTag MountRoleTag = FGameplayTag::RequestGameplayTag("Tame.Role.Mount");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Role")
	FGameplayTag CompanionRoleTag = FGameplayTag::RequestGameplayTag("Tame.Role.Companion");

	// Attempt tags
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Attempt")
	FGameplayTag InProgressTag = FGameplayTag::RequestGameplayTag("Tame.Attempt.InProgress");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Attempt")
	FGameplayTag SuccessTag = FGameplayTag::RequestGameplayTag("Tame.Attempt.Success");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Attempt")
	FGameplayTag FailTag = FGameplayTag::RequestGameplayTag("Tame.Attempt.Fail");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
	FGameplayTag TameAbilityTag;

	// AI Controllers for tamed roles
	UPROPERTY(EditDefaultsOnly, Category="Taming|AI")
	TSubclassOf<AAIController> TamedMountAIController;
	
	UPROPERTY(EditDefaultsOnly, Category="Taming|AI")
	TSubclassOf<AAIController> TamedCompanionAIController;

	// Tame requirements
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
	TSubclassOf<UACFItem> RequiredTamingItem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
	int32 RequiredItemCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
	TArray<FTameStatRequirement> StatRequirements;

	// Tame behavior
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	float TameDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	float RetryTameCooldown = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	bool bRunAwayOnFail = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	FGameplayTag RunawayActionTag = FGameplayTag::RequestGameplayTag("Actions.Flee");


	// Abilities granted on success (mount skills, commands, etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesGrantedWhenTamed;

	// Effects applied on success (e.g., loyalty buff)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
	TArray<TSubclassOf<UGameplayEffect>> EffectsOnTameSuccess;

	// Optional difficulty curve (X: level/temperament, Y: modifier 0..1)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Design")
	UCurveFloat* TameDifficultyCurve = nullptr;	
};
