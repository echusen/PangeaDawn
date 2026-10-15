// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BAGraph.h"

struct BLUEPRINTASSIST_API FBAGraphUtils
{
	static int StraightenPin(FBAPinPtr SourcePin, FBAPinPtr PinToAlign);
};
