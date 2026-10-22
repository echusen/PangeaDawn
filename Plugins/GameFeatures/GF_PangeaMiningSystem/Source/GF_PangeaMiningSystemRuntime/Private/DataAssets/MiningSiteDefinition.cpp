#include "DataAssets/MiningSiteDefinition.h"

bool UMiningSiteDefinition::GetLevelDefinition(const int32 Level, FMiningSiteLevelDefinition& OutDefinition) const
{
	for (const FMiningSiteLevelDefinition& Definition : Levels)
	{
		if (Definition.Level == Level)
		{
			OutDefinition = Definition;
			return true;
		}
	}

	return false;
}

int32 UMiningSiteDefinition::GetMaxLevel() const
{
	int32 MaxLevel = INDEX_NONE;
	for (const FMiningSiteLevelDefinition& Definition : Levels)
	{
		MaxLevel = FMath::Max(MaxLevel, Definition.Level);
	}

	return MaxLevel;
}

#if WITH_EDITOR
EDataValidationResult UMiningSiteDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;
	if (!FMath::IsFinite(SimulatedDaySeconds) || SimulatedDaySeconds < 1.0f ||
		!FMath::IsFinite(WorkerAttendanceRadius) || WorkerAttendanceRadius < 1.0f)
	{
		Context.AddError(FText::FromString(TEXT("SimulatedDaySeconds and WorkerAttendanceRadius must be finite and at least one.")));
		Result = EDataValidationResult::Invalid;
	}
	TSet<int32> SeenLevels;
	for (const FMiningSiteLevelDefinition& Level : Levels)
	{
		if (Level.Level < 0 || SeenLevels.Contains(Level.Level) || Level.StorageCapacity < 0 || Level.AutomatedMineralsPerDay < 0 ||
			Level.WorkerCount < 0 || Level.GuardCount < 0 || !FMath::IsFinite(Level.WorkerDeathChance) ||
			Level.WorkerDeathChance < 0.f || Level.WorkerDeathChance > 1.f ||
			!FMath::IsFinite(Level.ShipmentIntervalSeconds) || Level.ShipmentIntervalSeconds < 0.0f ||
			!FMath::IsFinite(Level.ShipmentLossChance) || Level.ShipmentLossChance < 0.0f || Level.ShipmentLossChance > 1.0f ||
			Level.MaxShipmentUnits < 0 ||
			((Level.AutomatedMineralsPerDay > 0 || Level.bShipmentUnlocked) && !Level.PrimaryOutput.ItemClass && !Level.PrimaryOutput.ItemTag.IsValid()))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Level %d has duplicate/invalid level, storage, production, output or shipment configuration."), Level.Level)));
			Result = EDataValidationResult::Invalid;
		}
		SeenLevels.Add(Level.Level);
		if (bUseHybridTransport && (!FMath::IsFinite(Level.TransportTravelSpeed) || Level.TransportTravelSpeed <= 0.f || Level.TransportEscortCount < 0 || Level.TransportEscortCount > 8 ||
			(!Level.TransportDraftAnimalClass.IsNull() && Level.TransportChariotClass.IsNull())))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Level %d has invalid hybrid speed/escort count, or a draft animal without a cart."), Level.Level)));
			Result = EDataValidationResult::Invalid;
		}
	}
	if (!SeenLevels.Contains(0))
	{
		Context.AddError(FText::FromString(TEXT("Mining site requires level 0.")));
		Result = EDataValidationResult::Invalid;
	}

	if (PresentationConfig.IsNull())
	{
		Context.AddError(FText::FromString(TEXT("PresentationConfig must be assigned.")));
		Result = EDataValidationResult::Invalid;
	}

	if (!PresentationConfig.IsNull() && !PresentationConfig.LoadSynchronous())
	{
		Context.AddError(FText::FromString(TEXT("PresentationConfig could not be loaded.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
