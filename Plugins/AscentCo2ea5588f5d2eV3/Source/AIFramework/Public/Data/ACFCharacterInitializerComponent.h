// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ACFAITypes.h"
#include "ACFCharacterInitializerComponent.generated.h"

class UACFCharacterDataAsset;


UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class AIFRAMEWORK_API UACFCharacterInitializerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UACFCharacterInitializerComponent();

	UFUNCTION(BlueprintCallable, Category = ACF)
	void InitFromDataAsset(UACFCharacterDataAsset* charData, int32 Level);

	// Apply all mesh data to the character
	UFUNCTION(BlueprintCallable, Category = ACF)
	void ApplyAllMeshData();

	// Check if the owner has been initialized
	UFUNCTION(BlueprintPure, Category = ACF)
	bool IsInitialized() const { return bHasInitialized; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InternalHandleServerInit(int32 Level);
	void InternalHandleClientInit();
	// Custom initialization logic for server and client
	UFUNCTION(BlueprintNativeEvent, Category = ACF)
	void HandleServerInit();

	UFUNCTION(BlueprintNativeEvent, Category = ACF)
	void HandleClientInit();

	// Auto - initialization on BeginPlay
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF|Auto Initialization")
	bool bAutoInit = false;

	// Soft reference to data asset for auto-initialization
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF|Auto Initialization",
		meta = (EditCondition = "bAutoInit", EditConditionHides))
	TSoftObjectPtr<UACFCharacterDataAsset> AutoInitDataAsset;

	// Default level for auto-initialization
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ACF|Auto Initialization",
		meta = (EditCondition = "bAutoInit", EditConditionHides, ClampMin = "1", ClampMax = "100"))
	int32 AutoInitLevel = 1;

	// Replicated data asset ID - more efficient than replicating the asset pointer
	UPROPERTY(ReplicatedUsing = OnRep_CharacterDataAssetId, BlueprintReadOnly, Category = "ACF|Character Data")
	FPrimaryAssetId CharacterDataAssetId;

	// Apply specific mesh data to a component
	void ApplyMeshDataToComponent(class USkeletalMeshComponent* Component,
		const FSkeletalMeshComponentData& MeshData);

	// Local cached data asset (not replicated)
	UPROPERTY(BlueprintReadOnly,EditAnywhere, Category = "ACF|Character Data")
	UACFCharacterDataAsset* CharacterDataAsset;

	// Called when data asset ID is replicated
	UFUNCTION()
	void OnRep_CharacterDataAssetId();

	UPROPERTY(Category = ACF, BlueprintReadOnly)
	AACFCharacter* OwningPawn;

private:

	// Load data asset from ID
	void LoadDataAssetFromId();

	void OnDataAssetLoaded();

	bool bHasInitialized = false;

	// Handle for async loading
	TSharedPtr<struct FStreamableHandle> LoadHandle;
};
