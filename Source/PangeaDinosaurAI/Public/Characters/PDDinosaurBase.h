// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ALSSavableInterface.h"
#include "Actors/ACFCharacter.h"
#include "Interfaces/PDDefinitionProviderInterface.h"
#include "Interfaces/PDCreatureIdentityInterface.h"
#include "PDDinosaurBase.generated.h"


class UALSLoadAndSaveComponent;
class UPangeaCreatureDefinition;
class UACFMountComponent;
class UACFVaultComponent;

/**
 * 
 */
UCLASS()
class PANGEADINOSAURAI_API APDDinosaurBase : public AACFCharacter,
	public IACFInteractableInterface,
	public IPDDefinitionProviderInterface,
	public IPDCreatureIdentityInterface
{
	GENERATED_BODY()

	//Constructor
	APDDinosaurBase(const FObjectInitializer& ObjectInitializer);

public:
	// Unreal Engine
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void ChangeVelocityState();
	virtual void Tick(float DeltaTime) override;
	
	// Interfaces
	virtual bool CanBeInteracted_Implementation(class APawn* Pawn) override;
	virtual void OnInteractedByPawn_Implementation(class APawn* Pawn, const FString& interactionType = "") override;
	virtual FText GetInteractableName_Implementation() override;
	virtual UPangeaCreatureDefinition* GetCreatureDefinition_Implementation() const override;
	virtual bool SetCreatureDefinition_Implementation(UPangeaCreatureDefinition* Definition) override;
	virtual FGuid GetCreatureId_Implementation() const override { return CreatureId; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	virtual TArray<UActorComponent*> GetComponentsToSave_Implementation() const override;
	

protected:
	void EnsureDefaultCombatStatRow(bool bReinitializeIfAssigned);
	void ConfigureCombatCollision();

	// Actor Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UACFMountComponent> MountComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UACFVaultComponent> VaultComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UALSLoadAndSaveComponent> ALSLoadAndSaveComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, SaveGame, Category="Definition", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPangeaCreatureDefinition> CreatureDefinition;

	/** Generated on authority at actor initialization, never on a CDO or snapshot query. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, SaveGame, Category="Identity")
	FGuid CreatureId;
	
	UFUNCTION(BlueprintCallable, Category="Dinosaur Movement")
	void Accelerate(float Value);
	
	UFUNCTION(BlueprintCallable, Category="Dinosaur Movement")
	void Brake(float Value);

public:
	virtual void OnLoaded_Implementation() override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
	bool bIsAccelerating;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
	bool bIsBraking;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
	FName HeadSocket;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
	float DefaultAcceleration = 10.0f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
	float DefaultDeceleration = -0.2f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category="Dinosaur Movement")
	TSoftClassPtr<AACFCharacter> PlayerRider;
};
