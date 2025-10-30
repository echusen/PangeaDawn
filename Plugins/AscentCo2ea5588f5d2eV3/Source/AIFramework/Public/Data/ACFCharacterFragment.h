// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ACFCharacterFragment.generated.h"


/*
 * Base class for pawn fragments used to compose pawn definitions in ACF
 * Can be extended in Blueprint to add custom data to characters.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class AIFRAMEWORK_API UACFCharacterFragment : public UObject
{
	GENERATED_BODY()

public:

	UACFCharacterFragment() {};

};
