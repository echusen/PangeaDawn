// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
	/** Execute the action. ContextObject is typically the owning entity (village actor) or player pawn depending on usage. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Upgrade")
	void Execute(UObject* ContextObject);
	virtual void Execute_Implementation(UObject* ContextObject);
};
