// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "FabQuixelAssetTypes.generated.h"

USTRUCT()
struct FFabSemanticTags
{
	GENERATED_BODY()

	UPROPERTY()
	FString Asset_Type;
};

USTRUCT()
struct FFabAssetMetaDataJson
{
	GENERATED_BODY()

	UPROPERTY()
	FString Id;

	UPROPERTY()
	TArray<FString> Categories;

	UPROPERTY()
	FFabSemanticTags SemanticTags;

	UPROPERTY()
	float Displacement_Bias_Tier1 = -1.0f;

	UPROPERTY()
	float Displacement_Scale_Tier1 = -1.0f;
};

UENUM()
enum class EFabMegascanImportType
{
	Model3D UMETA(DisplayName = "3D"),
	Surface UMETA(DisplayName = "Surface"),
	Decal UMETA(DisplayName = "Decal"),
	Imperfection UMETA(DisplayName = "Imperfection"),
	Plant UMETA(DisplayName = "Plant"),
};