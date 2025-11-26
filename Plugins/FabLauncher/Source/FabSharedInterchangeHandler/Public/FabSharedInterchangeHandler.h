// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangePipelineBase.h"

FABSHAREDINTERCHANGEHANDLER_API void ImportFabAssetWithInterchange(
	const FString& Source,
	const FString& Destination, 
	TFunction<void(TArray<UInterchangePipelineBase*>&, bool)> AdjustPipelinesFunction,
	TFunction<void(const TArray<UObject*>&)> Callback
);