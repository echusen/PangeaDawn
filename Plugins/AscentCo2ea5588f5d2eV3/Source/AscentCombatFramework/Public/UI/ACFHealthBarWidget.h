// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ACFHealthBarWidget.generated.h"

/**
 * 
 */
UCLASS()
class ASCENTCOMBATFRAMEWORK_API UACFHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
	public: 

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = ACF)
 	void SetupWithPawn(const APawn* pawn);
};
