// Copyright Epic Games, Inc. All Rights Reserved.
// From Fab plugin, UE 5.6
#pragma once

#include "CoreMinimal.h"
#include "InterchangeManager.h"

enum class EFabMegascanImportType;

class FABLAUNCHERQUIXELDEPENDENCIES_API FFabQuixelGltfImporter
{
private:
	static void SetupGlobalFoliageActor(const FString& ImportPath);
	static TArray<UInterchangePipelineBase*> GeneratePipelines(bool bHasInstancesOrComplexHierarchy);
public:
	static void ImportAsset(const FString& SourcePath, const FString& DestinationPath, EFabMegascanImportType ImportType, TFunction<void(const TArray<UObject*>&)> OnDone, bool bBuildNanite = true);
};