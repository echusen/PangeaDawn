#pragma once

#include "CoreMinimal.h"
#include "Components/ACFStorageComponent.h"
#include "UObject/Interface.h"
#include "Items/ACFItem.h"
#include "MiningSystemInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMiningSystemInterface : public UInterface
{
	GENERATED_BODY()
};

class PANGEAMININGSYSTEM_API IMiningSystemInterface
{
	GENERATED_BODY()

public:
	/**
	 * Request mined items from workstation
	 * Uses FInventoryItem (ACFU inventory system)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining")
	bool RequestMinedItems(AActor* Requester, TArray<FInventoryItem>& OutItems);

	/**
	 * Deposit items into storage
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining")
	bool DepositItems(AActor* Depositor, const TArray<FInventoryItem>& Items);

	/**
	 * Check if has items available
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining")
	bool HasItemsAvailable() const;

	/**
	 * Get storage component
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mining")
	UACFStorageComponent* GetStorageComponent() const;
};