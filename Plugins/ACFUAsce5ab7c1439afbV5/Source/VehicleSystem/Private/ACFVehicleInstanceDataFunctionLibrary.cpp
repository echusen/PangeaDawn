// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFVehicleInstanceDataFunctionLibrary.h"
#include "ACFVehicleInstanceData.h"
#include "Math/Float16.h"
#include "Components/PrimitiveComponent.h"

float UACFVehicleInstanceDataFunctionLibrary::BitcastIntToFloat(int32 Value)
{
	return reinterpret_cast<float&>(Value);
}

int32 UACFVehicleInstanceDataFunctionLibrary::BitcastFloatToInt(float Value)
{
	return reinterpret_cast<int32&>(Value);
}

float UACFVehicleInstanceDataFunctionLibrary::PackRandomFractionAndLightMask(float RandomFraction, int32 LightMask)
{
	// True half-float encode - matches FACFVehicleInstanceCustomData::RandomFraction,
	// which is documented as "Packed as FFloat16 into PackedParam1[0 : 15]".
	const FFloat16 HalfFraction(FMath::Clamp(RandomFraction, 0.0f, 1.0f));

	// Light flags are expected to already occupy bits 16-31 (unchanged from the existing
	// Select/Add chain), so we only need to mask off any stray low bits before combining.
	const uint32 PackedBits = (static_cast<uint32>(LightMask) & 0xFFFF0000u) | static_cast<uint32>(HalfFraction.Encoded);

	return reinterpret_cast<const float&>(PackedBits);
}

void UACFVehicleInstanceDataFunctionLibrary::UnpackRandomFractionAndLightMask(float PackedValue, float& OutRandomFraction, int32& OutLightMask)
{
	const uint32 PackedBits = reinterpret_cast<const uint32&>(PackedValue);

	FFloat16 HalfFraction;
	HalfFraction.Encoded = static_cast<uint16>(PackedBits & 0x0000FFFFu);
	OutRandomFraction = HalfFraction.GetFloat();

	OutLightMask = static_cast<int32>(PackedBits & 0xFFFF0000u);
}

bool UACFVehicleInstanceDataFunctionLibrary::GetCustomPrimitiveDataFloat(const UPrimitiveComponent* PrimitiveComponent, float& OutPackedParam1, int32 DataIndex)
{
	if (!PrimitiveComponent)
	{
		OutPackedParam1 = 0.0f;
		return false;
	}

	// Direct live read - no caching, no BP variable to forget to update: this always reflects
	// whatever was most recently written via Set Custom Primitive Data Float, by anything,
	// this frame or any prior frame.
	const TArray<float>& CustomPrimitiveData = PrimitiveComponent->GetCustomPrimitiveData().Data;
	if (CustomPrimitiveData.IsValidIndex(DataIndex))
	{
		OutPackedParam1 = CustomPrimitiveData[DataIndex];
		return true;
	}

	OutPackedParam1 = 0.0f;
	return false;
}

FACFPackedVehicleInstanceCustomData UACFVehicleInstanceDataFunctionLibrary::PackVehicleInstanceCustomData(const FACFVehicleInstanceCustomData& CustomData)
{
	return FACFPackedVehicleInstanceCustomData(CustomData);
}

FACFVehicleInstanceCustomData UACFVehicleInstanceDataFunctionLibrary::UnpackVehicleInstanceCustomData(const FACFPackedVehicleInstanceCustomData& PackedCustomData)
{
	return FACFVehicleInstanceCustomData(PackedCustomData);
}

bool UACFVehicleInstanceDataFunctionLibrary::GetPackedVehicleInstanceCustomData(const UPrimitiveComponent* PrimitiveComponent, FACFPackedVehicleInstanceCustomData& OutPackedCustomData, int32 DataIndex)
{
	if (!PrimitiveComponent)
	{
		OutPackedCustomData = FACFPackedVehicleInstanceCustomData();
		return false;
	}

	const TArray<float>& CustomPrimitiveData = PrimitiveComponent->GetCustomPrimitiveData().Data;
	if (CustomPrimitiveData.IsValidIndex(DataIndex))
	{
		OutPackedCustomData = FACFPackedVehicleInstanceCustomData(CustomPrimitiveData[DataIndex]);
		return true;
	}

	OutPackedCustomData = FACFPackedVehicleInstanceCustomData();
	return false;
}
