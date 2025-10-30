// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/PangeaBreedingSystemTypes.h"
#include "UObject/Interface.h"
#include "PangeaBreedingInterface.generated.h"

class UPangeaSpeciesDataAsset;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPangeaBreedingInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class PANGEABREEDINGSYSTEM_API IPangeaBreedingInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Breeding")
    FParentSnapshot GetParentSnapshot() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Breeding")
    bool IsFertile() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Breeding")
    bool SetFertile(bool bNewFertile);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Breeding")
    UPangeaSpeciesDataAsset* GetSpeciesData() const;

};
