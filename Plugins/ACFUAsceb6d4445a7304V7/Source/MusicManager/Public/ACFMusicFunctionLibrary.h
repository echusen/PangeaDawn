// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ACFMusicFunctionLibrary.generated.h"

class UACFMusicComponent;

/**
 * Shared music helpers available in both ACF Regular and ACF J.
 */
UCLASS()
class MUSICMANAGER_API UACFMusicFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Gets the music manager component from the first local player controller.
	 *
	 * @param WorldContextObject World context used to resolve the local player controller
	 * @return Music manager component, or nullptr if not found
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Music", meta = (WorldContext = "WorldContextObject"))
	static UACFMusicComponent* GetMusicManager(const UObject* WorldContextObject);
};
