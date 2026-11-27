// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "Items/ACFItem.h"

#include "TameSpeciesConfig.generated.h"

class AAIController;
class UACFItem;
class UCurveFloat;
class UGameplayAbility;
class UGameplayEffect;

/**
 * Defines a single stat requirement for taming.
 */
USTRUCT(BlueprintType)
struct FTameStatRequirement
{
	GENERATED_BODY()

	// GAS Attribute (e.g., Attributes.Dexterity)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taming|Requirements")
	FGameplayAttribute Attribute;

	// Optional UI label or design tag
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taming|Requirements")
	FGameplayTag DisplayTag;

	// Minimum required value for taming
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Taming|Requirements")
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

	/** Whether this species can take the Mount role once tamed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roles", SaveGame)
	bool bCanBeMount = true;

	/** Whether this species can take the Companion role once tamed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roles", SaveGame)
	bool bCanBeCompanion = true;

	/** AI controller that possesses the creature when it becomes a tamed mount. Leave empty to keep the current controller. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roles", SaveGame)
	TSubclassOf<AAIController> TamedMountAIController;

	/** AI controller that possesses the creature when it becomes a tamed companion. Leave empty to keep the current controller. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Roles", SaveGame)
	TSubclassOf<AAIController> TamedCompanionAIController;
#pragma endregion

#pragma region Tame Requirements
	// ------------------------------------------------------------
	// --- Tame Requirements ---
	// ------------------------------------------------------------

	/** Item the tamer must carry to attempt a tame. It is consumed on success. Leave empty for no item requirement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming|Requirements")
	TSubclassOf<UACFItem> RequiredTamingItem;

	/** How many of RequiredTamingItem the tamer must carry, and how many a successful tame consumes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming|Requirements")
	int32 RequiredItemCount = 1;

	/** Attribute minimums the tamer must meet to attempt a tame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming|Requirements")
	TArray<FTameStatRequirement> StatRequirements;
#pragma endregion

#pragma region Tame Behaviour
	// ------------------------------------------------------------
	// --- Tame Behavior ---
	// ------------------------------------------------------------

	/** Time limit of the taming minigame, in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming|Behavior", SaveGame)
	float TameDuration = 5.f;

	/** Seconds the fail tag stays on the tamer after a failed attempt, blocking a retry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming|Behavior", SaveGame)
	float RetryTameCooldown = 5.f;

	/** Design flag for fleeing after a failed tame. Native code does not read it: the flee action runs whenever RunawayActionTag is valid. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming|Behavior", SaveGame)
	bool bRunAwayOnFail = true;

	/** ACF action triggered on the creature after a failed tame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming|Behavior", SaveGame)
	FGameplayTag RunawayActionTag = FGameplayTag::RequestGameplayTag(TEXT("Actions.Flee"));

	/** Optional difficulty curve for the taming minigame. Native code does not read it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Taming|Behavior", SaveGame)
	TObjectPtr<UCurveFloat> TameDifficultyCurve = nullptr;
#pragma endregion

#pragma region GAS Integration
	// ------------------------------------------------------------
	// --- GAS Integration ---
	// ------------------------------------------------------------

	/** ACF action triggered on the tamer to start a tame attempt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	FGameplayTag TameAbilityTag;

	/** Abilities granted to the creature, on the authority, when it is tamed or restored as tamed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", SaveGame)
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesGrantedWhenTamed;

	/** Gameplay effects applied to the creature once, when a tame succeeds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", SaveGame)
	TArray<TSubclassOf<UGameplayEffect>> EffectsOnTameSuccess;
#pragma endregion

#pragma region Gameplay Tags
	// ------------------------------------------------------------
	// --- Gameplay Tags ---
	// ------------------------------------------------------------

	// State tags (used by AI / Behavior Trees)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|State", SaveGame)
	FGameplayTag WildStateTag = FGameplayTag::RequestGameplayTag(TEXT("Tame.State.Wild"));

	/** Loose tag on the creature while it is hostile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|State", SaveGame)
	FGameplayTag HostileStateTag = FGameplayTag::RequestGameplayTag(TEXT("Tame.State.Hostile"));

	/** Loose tag on the creature while it is tamed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|State", SaveGame)
	FGameplayTag TamedStateTag = FGameplayTag::RequestGameplayTag(TEXT("Tame.State.Tamed"));

	// Team tags (team switching on tame)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|Team", SaveGame)
	FGameplayTag WildTeamTag = FGameplayTag::RequestGameplayTag(TEXT("Teams.Neutral"));

	/** ACF team the creature joins when it is initialized as hostile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|Team", SaveGame)
	FGameplayTag HostileTeamTag = FGameplayTag::RequestGameplayTag(TEXT("Teams.Enemies"));

	/** ACF team the creature joins when it is tamed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|Team", SaveGame)
	FGameplayTag TamedTeamTag = FGameplayTag::RequestGameplayTag(TEXT("Teams.Heroes"));

	// Role tags (for identification)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|Role", SaveGame)
	FGameplayTag MountRoleTag = FGameplayTag::RequestGameplayTag(TEXT("Tame.Role.Mount"));

	/** Loose tag on the creature while its tamed role is Companion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|Role", SaveGame)
	FGameplayTag CompanionRoleTag = FGameplayTag::RequestGameplayTag(TEXT("Tame.Role.Companion"));

	// Attempt tags (used for cooldowns and state tracking)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|Attempt", SaveGame)
	FGameplayTag InProgressTag = FGameplayTag::RequestGameplayTag(TEXT("Tame.Attempt.InProgress"));

	/** Tag reserved for a successful attempt. Native code does not apply it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|Attempt", SaveGame)
	FGameplayTag SuccessTag = FGameplayTag::RequestGameplayTag(TEXT("Tame.Attempt.Success"));

	/** Loose tag on the tamer after a failed attempt. It blocks a retry until RetryTameCooldown has passed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags|Attempt", SaveGame)
	FGameplayTag FailTag = FGameplayTag::RequestGameplayTag(TEXT("Tame.Attempt.Fail"));
#pragma endregion
};
