// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PangeaFarmInterface.generated.h"

class APlayerController;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPangeaFarmInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by breeding farms so other systems can open the farm UI for a player
 * without depending on a concrete farm class.
 */
class PANGEABREEDINGSYSTEM_API IPangeaFarmInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/** Opens the breeding farm UI for the given player. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Breeding")
	void OpenFarmUI(APlayerController* Player);
};
