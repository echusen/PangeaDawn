// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/ACFItem.h"
#include "Objects/UpgradeAction.h"
#include "UA_GrantInventory.generated.h"

struct FBaseItem;
/**
 * 
 */
UCLASS(Blueprintable)
class PANGEABASEUPGRADESYSTEM_API UUA_GrantInventory : public UUpgradeAction
{
	GENERATED_BODY()
	
public:
	virtual bool ValidateConfiguration_Implementation(FText& OutFailure) const override;
	virtual bool CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const override;
	virtual bool TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) override;
	/** Items to grant (ItemId -> Count). Replace ItemId semantics with your inventory system's id type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
	TArray<FBaseItem> ItemsToGrant;

	virtual void ExecuteForContext(const FBaseUpgradeContext& Context) override;
	virtual void Execute_Implementation(UObject* ContextObject) override;
};
