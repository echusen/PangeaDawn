// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Pangea_DawnGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class APangea_DawnGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	APangea_DawnGameMode();
};



