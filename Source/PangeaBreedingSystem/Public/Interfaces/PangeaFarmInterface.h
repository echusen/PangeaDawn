// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PangeaFarmInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPangeaFarmInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class PANGEABREEDINGSYSTEM_API IPangeaFarmInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Breeding")
    void OpenFarmUI(APlayerController* Player);
};
