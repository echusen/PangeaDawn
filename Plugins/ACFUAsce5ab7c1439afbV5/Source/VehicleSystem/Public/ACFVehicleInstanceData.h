// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.
//
// Mirrors FMassTrafficVehicleInstanceCustomData / FMassTrafficPackedVehicleInstanceCustomData
// (same bit layout, same FFloat16 RandomFraction encode) but carries no dependency on the
// MassTraffic plugin, so vehicle paint/light visualization keeps working whether or not
// MassTraffic is enabled.

#pragma once

#include "CoreMinimal.h"
#include "ACFVehicleInstanceData.generated.h"

class UPrimitiveComponent;

/**
 * Unpacked vehicle visualization data: paint/flake RandomFraction plus 12 individual light flags.
 */
USTRUCT(BlueprintType)
struct VEHICLESYSTEM_API FACFVehicleInstanceCustomData
{
	GENERATED_BODY()

	FACFVehicleInstanceCustomData() = default;

	/** Unpacks straight from a packed float - use UACFVehicleInstanceDataFunctionLibrary::UnpackVehicleInstanceCustomData for a Pure BP node. */
	explicit FACFVehicleInstanceCustomData(const struct FACFPackedVehicleInstanceCustomData& PackedCustomData);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	float RandomFraction = 0.0f; // Packed as FFloat16 into PackedParam1[0 : 15]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bFrontLeftRunningLights = false; // PackedParam1[16 + 0]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bFrontRightRunningLights = false; // PackedParam1[16 + 1]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bRearLeftRunningLights = false; // PackedParam1[16 + 2]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bRearRightRunningLights = false; // PackedParam1[16 + 3]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bLeftBrakeLights = false; // PackedParam1[16 + 4]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bRightBrakeLights = false; // PackedParam1[16 + 5]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bLeftTurnSignalLights = false; // PackedParam1[16 + 6]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bRightTurnSignalLights = false; // PackedParam1[16 + 7]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bLeftHeadlight = false; // PackedParam1[16 + 8]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bRightHeadlight = false; // PackedParam1[16 + 9]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bReversingLights = false; // PackedParam1[16 + 10]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ACF|Vehicle")
	bool bAccessoryLights = false; // PackedParam1[16 + 11]
};

/**
 * FACFVehicleInstanceCustomData packed into a single 32-bit float, suitable for
 * Set Custom Primitive Data Float / Per-Instance Custom Data.
 */
USTRUCT(BlueprintType)
struct VEHICLESYSTEM_API FACFPackedVehicleInstanceCustomData
{
	GENERATED_BODY()

	FACFPackedVehicleInstanceCustomData() = default;

	explicit FACFPackedVehicleInstanceCustomData(const float InPackedParam1)
		: PackedParam1(InPackedParam1) {}

	/** Packs straight from unpacked data - use UACFVehicleInstanceDataFunctionLibrary::PackVehicleInstanceCustomData for a Pure BP node. */
	explicit FACFPackedVehicleInstanceCustomData(const FACFVehicleInstanceCustomData& UnpackedCustomData);

	/**
	 * Bit packed param: [ 0000000000000000 | VisualizationFlags (bits 16-27) | RandomFraction as FFloat16 (bits 0-15) ]
	 */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ACF|Vehicle")
	float PackedParam1 = 0.0f;
};
