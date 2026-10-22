// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/BaseUpgradeContext.h"
#include "UObject/Object.h"
#include "UpgradeAction.generated.h"

/**
 * 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class PANGEABASEUPGRADESYSTEM_API UUpgradeAction : public UObject
{
	GENERATED_BODY()
	
public:
	/** Optional authored identity. None uses the existing inline object's relative path, never its array index.
	 * Preserve this identity (and the definition path) when editing a released definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FName ActionId;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Upgrade")
	bool ValidateConfiguration(FText& OutFailure) const;
	virtual bool ValidateConfiguration_Implementation(FText& OutFailure) const;

	/** Pure preflight contract: implementations must not mutate gameplay state. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Upgrade")
	bool CanExecuteUpgrade(const FBaseUpgradeContext& Context, FText& OutFailure) const;
	virtual bool CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const;

	/** Must report actual completion, not merely dispatch a void event or asynchronous command. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintAuthorityOnly, Category="Upgrade")
	bool TryExecuteUpgrade(const FBaseUpgradeContext& Context, FText& OutFailure);
	virtual bool TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure);

	/** Opt in only for repeatable absolute-state reconstruction, never inventory grants or toggles. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Upgrade")
	bool CanReconstructOnLoad() const;
	virtual bool CanReconstructOnLoad_Implementation() const;

	/** Logical facility unlock advertised to UI, independent of a physical facility group. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Upgrade")
	FGameplayTag GetUnlockedFacilityTag() const;
	virtual FGameplayTag GetUnlockedFacilityTag_Implementation() const;

	virtual void ExecuteForContext(const FBaseUpgradeContext& Context);

	/** Execute the action. ContextObject is typically the owning entity (village actor) or player pawn depending on usage. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Upgrade")
	void Execute(UObject* ContextObject);
	virtual void Execute_Implementation(UObject* ContextObject);
};
