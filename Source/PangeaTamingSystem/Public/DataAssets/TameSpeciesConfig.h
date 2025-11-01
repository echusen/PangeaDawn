// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Items/ACFItem.h"
#include "TameSpeciesConfig.generated.h"

class UACFItem;
class UGameplayAbility;
class UGameplayEffect;
class AAIController;

/**
 * Defines a single stat requirement for taming.
 */
USTRUCT(BlueprintType)
struct FTameStatRequirement
{
	GENERATED_BODY()

	// GAS Attribute (e.g., Attributes.Dexterity)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Taming|Requirements")
	FGameplayAttribute Attribute;

	// Optional UI label or design tag
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Taming|Requirements")
	FGameplayTag DisplayTag;

	// Minimum required value for taming
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Taming|Requirements")
	float MinValue = 0.f;
};


/**
 * Species configuration asset that defines all taming parameters and behavior.
 */
UCLASS(BlueprintType)
class PANGEATAMINGSYSTEM_API UTameSpeciesConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	
#pragma region Roles
	// ------------------------------------------------------------
	// --- Roles ---
	// ------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
	bool bCanBeMount = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
	bool bCanBeCompanion = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
	TSubclassOf<AAIController> TamedMountAIController;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Roles")
	TSubclassOf<AAIController> TamedCompanionAIController;
#pragma endregion

#pragma region Tame Requirements
	// ------------------------------------------------------------
	// --- Tame Requirements ---
	// ------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
	TSubclassOf<UACFItem> RequiredTamingItem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
	int32 RequiredItemCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Requirements")
	TArray<FTameStatRequirement> StatRequirements;
#pragma endregion

#pragma region Tame Behaviour
	// ------------------------------------------------------------
	// --- Tame Behavior ---
	// ------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	float TameDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	float RetryTameCooldown = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	bool bRunAwayOnFail = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	FGameplayTag RunawayActionTag = FGameplayTag::RequestGameplayTag("Actions.Flee");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Taming|Behavior")
	UCurveFloat* TameDifficultyCurve = nullptr;
#pragma endregion

#pragma region GAS Integration
	// ------------------------------------------------------------
	// --- GAS Integration ---
	// ------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
	FGameplayTag TameAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesGrantedWhenTamed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS")
	TArray<TSubclassOf<UGameplayEffect>> EffectsOnTameSuccess;
#pragma endregion

#pragma region Gameplay Tags
	// ------------------------------------------------------------
	// --- Gameplay Tags ---
	// ------------------------------------------------------------

	// State tags (used by AI / Behavior Trees)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|State")
	FGameplayTag WildStateTag = FGameplayTag::RequestGameplayTag("Tame.State.Wild");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|State")
	FGameplayTag HostileStateTag = FGameplayTag::RequestGameplayTag("Tame.State.Hostile");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|State")
	FGameplayTag TamedStateTag = FGameplayTag::RequestGameplayTag("Tame.State.Tamed");

	// Team tags (team switching on tame)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Team")
	FGameplayTag WildTeamTag = FGameplayTag::RequestGameplayTag("Teams.Neutral");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Team")
	FGameplayTag HostileTeamTag = FGameplayTag::RequestGameplayTag("Teams.Enemies");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Team")
	FGameplayTag TamedTeamTag = FGameplayTag::RequestGameplayTag("Teams.Heroes");

	// Role tags (for identification)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Role")
	FGameplayTag MountRoleTag = FGameplayTag::RequestGameplayTag("Tame.Role.Mount");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Role")
	FGameplayTag CompanionRoleTag = FGameplayTag::RequestGameplayTag("Tame.Role.Companion");

	// Attempt tags (used for cooldowns and state tracking)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Attempt")
	FGameplayTag InProgressTag = FGameplayTag::RequestGameplayTag("Tame.Attempt.InProgress");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Attempt")
	FGameplayTag SuccessTag = FGameplayTag::RequestGameplayTag("Tame.Attempt.Success");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags|Attempt")
	FGameplayTag FailTag = FGameplayTag::RequestGameplayTag("Tame.Attempt.Fail");
#pragma endregion
};
