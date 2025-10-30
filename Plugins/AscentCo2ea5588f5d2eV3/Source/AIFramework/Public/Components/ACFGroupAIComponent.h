// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFAITypes.h"
#include "Actors/ACFCharacter.h"
#include "CoreMinimal.h"
#include <Components/PrimitiveComponent.h>
#include <Components/SceneComponent.h>
#include <Components/SplineComponent.h>
#include <GameplayTagContainer.h>

#include "ACFGroupAIComponent.generated.h"

struct FAISpawnInfo;
class FPrimitiveSceneProxy;
struct FStreamableHandle;
class AACFCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAgentDeath, const AACFCharacter*, character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllAgentDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAgentsSpawned);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAgentsChanged);

/**
 * * Component responsible for managing AI groups in ACF.
 */
UCLASS(ClassGroup = (ACF), Blueprintable, meta = (BlueprintSpawnableComponent))
class AIFRAMEWORK_API UACFGroupAIComponent : public UPrimitiveComponent {
	GENERATED_BODY()

public:
	/**
	 * * Default constructor.
	 */
	UACFGroupAIComponent();

protected:
	/**
	 * * Called when the game starts.
	 */
	virtual void BeginPlay() override;

	/**
	 * * Sets internal references for AI management.
	 */
	virtual void SetReferences();

protected:
	/**
	 * * Called when an AI character in the group dies.
	 * @param character Pointer to the deceased AI character.
	 */
	UFUNCTION()
	virtual void OnChildDeath(const AACFCharacter* character);

	/**
	 * * The group name used for UI representation.
	 */
	UPROPERTY(EditAnywhere, Category = "ACF|AI Config")
	FName GroupName = "Default Group Name";

	/**
	 * * Whether to override individual agent perception settings.
	 */
	UPROPERTY(EditAnywhere, Category = "ACF|AI Config")
	bool bOverrideAgentPerception = true;

	/**
	 * * Whether to alert other team members upon engagement.
	 */
	UPROPERTY(EditAnywhere, Category = "ACF|AI Config")
	bool bAlertOtherTeamMembers = true;

	/**
	 * * Whether to override the individual agent team settings.
	 */
	UPROPERTY(EditAnywhere, Category = "ACF|AI Config")
	bool bOverrideAgentTeam = true;

	/**
	 * * The combat team the AI group belongs to.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Teams"), Category = "ACF|AI Config")
	FGameplayTag CombatTeam;

	/**
	 * * The default AI state for the group when spawned.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|AI Config")
	FGameplayTag DefaultAiState;

	/**
	 * * The maximum number of simultaneous AI agents allowed in the group.
	 */
	UPROPERTY(EditAnywhere, Savegame, BlueprintReadOnly, Category = "ACF|Spawn")
	int32 MaxSimultaneousAgents = 20;

	/**
	 * * Whether the group can spawn multiple times.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|Spawn")
	bool bCanSpawnMultitpleTimes = false;

	/**
	 * * Default offset for AI spawning locations.
	 */
	UPROPERTY(EditAnywhere, Category = "ACF|Spawn")
	FVector2D DefaultSpawnOffset;

	/**
	 * * List of AI characters to spawn in the group.
	 */
	UPROPERTY(EditAnywhere, SaveGame, meta = (TitleProperty = "AIClassBP"), BlueprintReadWrite, Category = "ACF|Spawn")
	TArray<FAISpawnInfo> AIToSpawn;

	/**
	 * * The leader of the AI group.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = ACF)
	TObjectPtr<class AActor> groupLead;

	/**
	 * * List of AI agents currently in the group.
	 */
	UPROPERTY(SaveGame, Replicated)
	TArray<FAIAgentsInfo> AICharactersInfo;

	/**
	 * * Called when the component is fully loaded.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ACF)
	void OnComponentLoaded();

	/**
	 * * Whether the AI group is currently engaged in combat.
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = ACF)
	bool bInBattle;

public:
	/**
	 * * Event triggered when an AI agent in the group dies.
	 */
	UPROPERTY(BlueprintAssignable, Category = ACF)
	FOnAgentDeath OnAgentDeath;

	/**
	 * * Event triggered when all AI agents in the group are dead.
	 */
	UPROPERTY(BlueprintAssignable, Category = ACF)
	FOnAllAgentDeath OnAllAgentDeath;

	/**
	 * * Event triggered when AI agents are spawned in the group.
	 */
	UPROPERTY(BlueprintAssignable, Category = ACF)
	FOnAgentsSpawned OnAgentsSpawned;

	/**
	 * * Event triggered when AI agents are despawned.
	 */
	UPROPERTY(BlueprintAssignable, Category = ACF)
	FOnAgentsSpawned OnAgentsDespawned;

	/**
	 * * Event triggered when AI agents change in the group.
	 */
	UPROPERTY(BlueprintAssignable, Category = ACF)
	FOnAgentsChanged OnAgentsChanged;

	/**
	 * * Retrieves the nearest AI agent to a given location.
	 * @param location The target location.
	 * @return Pointer to the closest AI agent.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	class AACFCharacter* GetAgentNearestTo(const FVector& location) const;

	/**
	 * * Checks if the group is currently engaged in battle.
	 * @return True if in battle, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	FORCEINLINE bool IsInBattle() const { return bInBattle; }

	/**
	 * * Retrieves the combat team of the group.
	 * @return The combat team enum value.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	FGameplayTag GetCombatTeam() const;

	/**
	 * * Retrieves the total number of agents in the group.
	 * @return The number of agents in the group.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	FORCEINLINE int32 GetGroupSize() const { return AICharactersInfo.Num(); }

	/**
	 * * Removes an agent from the group.
	 * @param character Pointer to the agent to be removed.
	 * @return True if successfully removed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	bool RemoveAgentFromGroup(AACFCharacter* character);

	/**
	 * * Retrieves all agents in the group.
	 * @param outAgents Output parameter containing the list of agents.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void GetGroupAgents(TArray<FAIAgentsInfo>& outAgents) const { outAgents = AICharactersInfo; }

	/**
	 * * Sets the battle state of the group.
	 * @param inBattle True to engage the group in battle, false otherwise.
	 * @param newTarget The target to engage if in battle.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetInBattle(bool inBattle, AActor* newTarget);

	/**
	 * * Adds an AI to the spawn list from a class reference.
	 * @param charClass The class of the AI to be added.
	 * @return True if successfully added, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	bool AddAIToSpawnFromClass(const TSubclassOf<AACFCharacter>& charClass);

	/**
	 * * Adds an AI to the spawn list.
	 * @param spawnInfo The AI spawn information.
	 * @return True if successfully added, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	bool AddAIToSpawn(const FAISpawnInfo& spawnInfo);

	/**
	 * * Removes an AI from the spawn list by class reference.
	 * @param charClass The class of the AI to be removed.
	 * @return True if successfully removed, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	bool RemoveAIToSpawn(const TSubclassOf<AACFCharacter>& charClass);

	/**
	 * * Replaces the list of AI to spawn with a new list.
	 * @param newAIs The new list of AI spawn information.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void ReplaceAIToSpawn(const TArray<FAISpawnInfo>& newAIs);

	/**
	 * * Retrieves an agent by its index in the group.
	 * @param index The index of the agent.
	 * @param outAgent Output parameter containing the agent's information.
	 * @return True if the agent was found, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	bool GetAgentByIndex(int32 index, FAIAgentsInfo& outAgent) const;

	/**
	 * * Retrieves the enemy group associated with this group.
	 * @return A pointer to the enemy group component.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	class UACFGroupAIComponent* GetEnemyGroup() const { return enemyGroup; }

	/**
	 * * Retrieves the leader of the group.
	 * @return A pointer to the group leader actor.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	class AActor* GetGroupLead() const { return groupLead; }

	/**
	 * * Calculates and retrieves the centroid position of the group.
	 * @return The centroid location of the group.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	FVector GetGroupCentroid() const;

	/**
	 * * Requests a new target for an AI agent.
	 * @param requestSender The AI controller requesting a new target.
	 * @return A pointer to the new target character.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	class AACFCharacter* RequestNewTarget(const AACFAIController* requestSender);

	/**
	 * * Sends a command to all companions in the group.
	 * @param command The gameplay tag representing the command to be executed.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = ACF)
	void SendCommandToCompanions(FGameplayTag command);

	/**
	 * * Spawns the AI group.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = ACF)
	void SpawnGroup();

	/**
	 * * Despawns the AI group.
	 * @param bUpdateAIToSpawn Whether to update the AI spawn list.
	 * @param actionToTriggerOnDyingAgent Optional action tag for dying agents.
	 * @param lifespawn Time before despawn is completed.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = ACF)
	void DespawnGroup(const bool bUpdateAIToSpawn = true, FGameplayTag actionToTriggerOnDyingAgent = FGameplayTag(), float lifespawn = .2f);

	/**
	 * * Checks if other team members should be alerted.
	 * @return True if alerting is enabled, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	bool GetAlertOtherTeamMembers() const { return bAlertOtherTeamMembers; }

	/**
	 * * Sets whether other team members should be alerted.
	 * @param val True to enable alerting, false to disable.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetAlertOtherTeamMembers(bool val) { bAlertOtherTeamMembers = val; }

	/**
	 * * Retrieves the list of AI to spawn.
	 * @return An array containing the AI spawn information.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	TArray<FAISpawnInfo> GetAIToSpawn() const { return AIToSpawn; }

	/**
	 * * Gets the total number of AI configured to spawn.
	 * @return The total AI count to be spawned.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	int32 GetTotalAIToSpawnCount() const;

	/**
	 * * Gets the total number of agents currently spawned.
	 * @return The current agent count.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	int32 GetTotalAgentsCount() const { return AICharactersInfo.Num(); }

	/**
	 * * Adds an existing character to the AI group.
	 * @param character Pointer to the character to be added.
	 * @return True if successfully added, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	bool AddExistingCharacterToGroup(AACFCharacter* character);

	/**
	 * * Reinitializes a given AI agent.
	 * @param character Pointer to the AI agent to be reinitialized.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void ReInitAgent(AACFCharacter* character);

	/**
	 * * Checks if a given character is already part of the group.
	 * @param character Pointer to the character.
	 * @return True if the character is in the group, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	bool IsAlreadyInGroup(const AACFCharacter* character) const { return AICharactersInfo.Contains(character); }

	/**
	 * * Checks if multiple spawns are allowed for this group.
	 * @return True if multiple spawns are enabled, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	bool CanSpawnMultitpleTimes() const { return bCanSpawnMultitpleTimes; }

	/**
	 * * Enables or disables multiple spawns for this group.
	 * @param bEnabled True to allow multiple spawns, false to disable.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetCanSpawnMultitpleTimes(bool bEnabled) { bCanSpawnMultitpleTimes = bEnabled; }

	/**
	 * * Retrieves the maximum number of simultaneous agents.
	 * @return The maximum number of agents allowed.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	int32 GetMaxSimultaneousAgents() const { return MaxSimultaneousAgents; }

	/**
	 * * Sets the maximum number of simultaneous agents.
	 * @param val The new maximum number of agents.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetMaxSimultaneousAgents(int32 val) { MaxSimultaneousAgents = val; }

	/**
	 * * Retrieves the group's name.
	 * @return The name of the group.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	FName GetGroupName() const { return GroupName; }

	/**
	 * * Checks if the group is currently spawned.
	 * @return True if the group is spawned, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = ACF)
	bool IsGroupSpawned() const { return bAlreadySpawned; }

	/**
	 * * Sets the group's name.
	 * @param val The new name for the group.
	 */
	UFUNCTION(BlueprintCallable, Category = ACF)
	void SetGroupName(FName val) { GroupName = val; }

	/**
	 * * Initializes AI agents within the group.
	 */
	void InitAgents();


	//RPC VERSION FOR MULTIPLAYER!

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
	void ServerAddCharacterToGroup(AACFCharacter* character);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
	void ServerRemoveCharacterToGroup(AACFCharacter* character);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = ACF)
	void ServerAddAIToSpawn(const FAISpawnInfo& spawnInfo);

	//END RPC!

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditComponentMove(bool bFinished) override;
#endif
private:
	UPROPERTY(SaveGame)
	bool bAlreadySpawned = false;

	void Internal_SendCommandToAgents(FGameplayTag command);

	UPROPERTY()
	TObjectPtr<class UACFGroupAIComponent> enemyGroup;

	void Internal_SpawnGroup();

	uint8 AddAgentToGroup(const FAISpawnInfo& spawnInfo);
	void InitAgent(FAIAgentsInfo& agent, int32 childIndex);

	void SetEnemyGroup(UACFGroupAIComponent* inEnemyGroup);

	UFUNCTION()
	void HandleAgentDeath(class AACFCharacter* agent);

	void OnAIAssetsLoaded();

protected:
	// Handle for managing async loading
	TSharedPtr<FStreamableHandle> StreamableHandle;
};
