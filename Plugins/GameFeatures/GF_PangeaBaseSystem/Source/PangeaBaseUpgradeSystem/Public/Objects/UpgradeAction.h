// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/BaseUpgradeContext.h"
#include "UObject/Object.h"
#include "UpgradeAction.generated.h"

/**
 * Base class for one effect that a milestone applies when a base level is upgraded.
 * Actions are authored inline in the upgrade definition and must report whether they really completed,
 * so a failed or partial upgrade is never recorded as a success.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class PANGEABASEUPGRADESYSTEM_API UUpgradeAction : public UObject
{
	GENERATED_BODY()

public:
	/** Optional authored identity. None uses the existing inline object's relative path, never its array index.
	 * Preserve this identity (and the definition path) when editing a released definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	FName ActionId;

	/** Returns false with a reason when the authored data of the action is not usable. The base class always fails. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	bool ValidateConfiguration(FText& OutFailure) const;
	virtual bool ValidateConfiguration_Implementation(FText& OutFailure) const;

	/** Pure preflight contract: implementations must not mutate gameplay state. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	bool CanExecuteUpgrade(const FBaseUpgradeContext& Context, FText& OutFailure) const;
	virtual bool CanExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure) const;

	/** Must report actual completion, not merely dispatch a void event or asynchronous command. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintAuthorityOnly, Category = "Upgrade")
	bool TryExecuteUpgrade(const FBaseUpgradeContext& Context, FText& OutFailure);
	virtual bool TryExecuteUpgrade_Implementation(const FBaseUpgradeContext& Context, FText& OutFailure);

	/** Opt in only for repeatable absolute-state reconstruction, never inventory grants or toggles. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	bool CanReconstructOnLoad() const;
	virtual bool CanReconstructOnLoad_Implementation() const;

	/** Logical facility unlock advertised to UI, independent of a physical facility group. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	FGameplayTag GetUnlockedFacilityTag() const;
	virtual FGameplayTag GetUnlockedFacilityTag_Implementation() const;

	/** Legacy entry point without a result. The default forwards to Execute with the preferred action object. */
	virtual void ExecuteForContext(const FBaseUpgradeContext& Context);

	/** Execute the action. ContextObject is typically the owning entity (village actor) or player pawn depending on usage. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Upgrade")
	void Execute(UObject* ContextObject);
	virtual void Execute_Implementation(UObject* ContextObject);
};
