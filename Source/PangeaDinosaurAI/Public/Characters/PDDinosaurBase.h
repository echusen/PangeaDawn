// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ALSSavableInterface.h"
#include "TamingTypes.h"
#include "Actors/ACFCharacter.h"
#include "PDDinosaurBase.generated.h"


class UPangeaBreedableComponent;
class UPangeaTamingComponent;
class UACFMountComponent;
class UACFVaultComponent;

/**
 * 
 */
UCLASS()
class PANGEADINOSAURAI_API APDDinosaurBase : public AACFCharacter,
	public IACFInteractableInterface,
	public IALSSavableInterface
{
	GENERATED_BODY()

	//Constructor
	APDDinosaurBase(const FObjectInitializer& ObjectInitializer);

public:
	// Unreal Engine
	virtual void BeginPlay() override;
	void ChangeVelocityState();
	virtual void Tick(float DeltaTime) override;
	
	// Interfaces
	virtual bool CanBeInteracted_Implementation(class APawn* Pawn) override;
	virtual void OnInteractedByPawn_Implementation(class APawn* Pawn, const FString& interactionType = "") override;
	virtual FText GetInteractableName_Implementation() override;

protected:
	// Actor Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPangeaBreedableComponent> BreedableComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPangeaTamingComponent> TamingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UACFMountComponent> MountComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UACFVaultComponent> VaultComponent;
	
	UFUNCTION(BlueprintCallable, Category="Dinosaur Movement")
	void Accelerate(float Value);
	
	UFUNCTION(BlueprintCallable, Category="Dinosaur Movement")
	void Brake(float Value);

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
