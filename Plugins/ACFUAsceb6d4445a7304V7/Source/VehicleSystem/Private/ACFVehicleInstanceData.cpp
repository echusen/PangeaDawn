// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.
//
// Same reinterpret_cast / FFloat16 encode / bit shifts as MassTraffic's
// MassTrafficVehicleVisualizationProcessor.cpp - only the struct/field names changed.

#include "ACFVehicleInstanceData.h"
#include "Math/Float16.h"

FACFVehicleInstanceCustomData::FACFVehicleInstanceCustomData(const FACFPackedVehicleInstanceCustomData& PackedCustomData)
{
	const uint32& PackedParam1AsUint32 = reinterpret_cast<const uint32&>(PackedCustomData.PackedParam1);

	// Decode RandomFraction from the 16 least significant bits
	FFloat16 HalfPrecisionRandomFraction;
	HalfPrecisionRandomFraction.Encoded = static_cast<uint16>(PackedParam1AsUint32 & 0x0000FFFFu);
	RandomFraction = HalfPrecisionRandomFraction;

	bFrontLeftRunningLights = (PackedParam1AsUint32 & (1UL << (16 + 0))) != 0;
	bFrontRightRunningLights = (PackedParam1AsUint32 & (1UL << (16 + 1))) != 0;
	bRearLeftRunningLights = (PackedParam1AsUint32 & (1UL << (16 + 2))) != 0;
	bRearRightRunningLights = (PackedParam1AsUint32 & (1UL << (16 + 3))) != 0;
	bLeftBrakeLights = (PackedParam1AsUint32 & (1UL << (16 + 4))) != 0;
	bRightBrakeLights = (PackedParam1AsUint32 & (1UL << (16 + 5))) != 0;
	bLeftTurnSignalLights = (PackedParam1AsUint32 & (1UL << (16 + 6))) != 0;
	bRightTurnSignalLights = (PackedParam1AsUint32 & (1UL << (16 + 7))) != 0;
	bLeftHeadlight = (PackedParam1AsUint32 & (1UL << (16 + 8))) != 0;
	bRightHeadlight = (PackedParam1AsUint32 & (1UL << (16 + 9))) != 0;
	bReversingLights = (PackedParam1AsUint32 & (1UL << (16 + 10))) != 0;
	bAccessoryLights = (PackedParam1AsUint32 & (1UL << (16 + 11))) != 0;
}

FACFPackedVehicleInstanceCustomData::FACFPackedVehicleInstanceCustomData(const FACFVehicleInstanceCustomData& UnpackedCustomData)
{
	uint32& PackedParam1AsUint32 = reinterpret_cast<uint32&>(PackedParam1);

	// Encode RandomFraction as 16-bit float in the 16 least significant bits
	const FFloat16 HalfPrecisionRandomFraction = FMath::Clamp(UnpackedCustomData.RandomFraction, 0.0f, 1.0f);
	PackedParam1AsUint32 = static_cast<uint32>(HalfPrecisionRandomFraction.Encoded);

	if (UnpackedCustomData.bFrontLeftRunningLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 0);
	}
	if (UnpackedCustomData.bFrontRightRunningLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 1);
	}
	if (UnpackedCustomData.bRearLeftRunningLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 2);
	}
	if (UnpackedCustomData.bRearRightRunningLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 3);
	}
	if (UnpackedCustomData.bLeftBrakeLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 4);
	}
	if (UnpackedCustomData.bRightBrakeLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 5);
	}
	if (UnpackedCustomData.bLeftTurnSignalLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 6);
	}
	if (UnpackedCustomData.bRightTurnSignalLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 7);
	}
	if (UnpackedCustomData.bLeftHeadlight)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 8);
	}
	if (UnpackedCustomData.bRightHeadlight)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 9);
	}
	if (UnpackedCustomData.bReversingLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 10);
	}
	if (UnpackedCustomData.bAccessoryLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 11);
	}
}
