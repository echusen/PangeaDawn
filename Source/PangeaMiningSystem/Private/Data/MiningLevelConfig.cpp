// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/MiningLevelConfig.h"

const FMiningLevelDefinition& UMiningLevelConfig::GetLevelDefinitionChecked(int32 Index) const
{
	// Fallback to first level if out of range, to avoid crashes in shipping.
	if (!Levels.IsValidIndex(Index))
	{
		static FMiningLevelDefinition Dummy;
		return Levels.Num() > 0 ? Levels[0] : Dummy;
	}

	return Levels[Index];
}