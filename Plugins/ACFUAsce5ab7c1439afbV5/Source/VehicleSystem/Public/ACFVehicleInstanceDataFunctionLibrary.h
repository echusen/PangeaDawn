// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ACFVehicleInstanceData.h"
#include "ACFVehicleInstanceDataFunctionLibrary.generated.h"

class UPrimitiveComponent;

/**
 * BlueprintPure helpers that do TRUE bit-reinterpretation (bit-cast), not numeric conversion.
 *
 * Blueprint's native "Int to Float" / "Float to Int" nodes perform a numeric VALUE conversion
 * (e.g. the integer 1065353216 becomes the float 1065353216.0). That is NOT the same operation
 * as C++'s reinterpret_cast<float&>(IntValue), which reads the same 4 raw bytes back as a
 * different type without changing a single bit.
 *
 * This distinction matters here because materials read Custom Primitive Data via HLSL's
 * asuint()/f16tof32(), which expect the float's raw bit pattern to already encode packed
 * integer flags (see FMassTrafficPackedVehicleInstanceCustomData in MassTraffic). Building
 * that packed value with ordinary Blueprint math produces a numerically different float whose
 * bit pattern has nothing to do with the flags you intended - hence lights/colors reading as
 * wrong or seemingly-random combinations even though the pack/unpack math "looks" correct.
 *
 * Use BitcastIntToFloat as the final step of Pack (replacing "Int to Float"), and
 * BitcastFloatToInt as the first step of Unpack (replacing "Round (float to int)"),
 * to exactly match the original C++/material bit layout with zero material changes required.
 */
UCLASS(DisplayName = "ACF Vehicle Instance Data Function Library")
class VEHICLESYSTEM_API UACFVehicleInstanceDataFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Reinterprets the raw 32 bits of an integer as a float, with no numeric conversion.
	 * Equivalent to C++'s reinterpret_cast<float&>(Value).
	 * Use this as the final node in Pack, feeding the result into PackedParam1.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Vehicle Instance Data")
	static float BitcastIntToFloat(int32 Value);

	/**
	 * Reinterprets the raw 32 bits of a float as an integer, with no numeric conversion.
	 * Equivalent to C++'s reinterpret_cast<int32&>(Value).
	 * Use this as the first node in Unpack, feeding PackedParam1 in before any bitwise AND.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Vehicle Instance Data")
	static int32 BitcastFloatToInt(float Value);

	/**
	 * Packs a 0.0-1.0 RandomFraction (encoded as a true FFloat16 bit pattern) into the low 16
	 * bits, combined with a pre-built light-flags mask (bits 16-31, e.g. the output of a
	 * Select/Add chain) into a single float suitable for Set Custom Primitive Data Float.
	 *
	 * Use this as the final node in Pack in place of BitcastIntToFloat, feeding both the
	 * RandomFraction variable and your existing LightMask integer sum into it.
	 *
	 * @param RandomFraction	0.0-1.0 paint/color variation value. Clamped internally.
	 * @param LightMask			Integer sum of light bit-flags, already offset into bits 16-31
	 *							(e.g. bLeftBrakeLights contributes 1048576, etc - unchanged from
	 *							your existing Pack logic).
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Vehicle Instance Data")
	static float PackRandomFractionAndLightMask(float RandomFraction, int32 LightMask);

	/**
	 * Unpacks a single Custom Primitive Data float into its RandomFraction (true FFloat16
	 * decode) and LightMask (bits 16-31, ready for your existing Bitwise AND chain) parts.
	 *
	 * Use this as the first node in Unpack in place of BitcastFloatToInt - wire
	 * OutLightMask into whatever previously consumed the raw bitcast integer.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Vehicle Instance Data")
	static void UnpackRandomFractionAndLightMask(float PackedValue, float& OutRandomFraction, int32& OutLightMask);

	/**
	 * Real, direct read of a primitive component's live Custom Primitive Data - no caching,
	 * no BP variable, no risk of a stale/never-updated value. Blueprint has no native node for
	 * this because UPrimitiveComponent::GetCustomPrimitiveData() is C++-only; this exposes it.
	 *
	 * @param PrimitiveComponent	The component to read Custom Primitive Data from (e.g. SM Frame).
	 * @param DataIndex				The CPD index to read. 1 matches MassTraffic's convention.
	 * @param OutPackedParam1		The raw packed float at that index if found, otherwise 0.0.
	 * @return						true if CustomPrimitiveData had a valid entry at DataIndex.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Vehicle Instance Data")
	static bool GetCustomPrimitiveDataFloat(const UPrimitiveComponent* PrimitiveComponent, float& OutPackedParam1, int32 DataIndex = 1);

	// ------------------------------------------------------------------------------------------
	// Struct-based equivalents of the raw-float functions above, with all bit math done natively
	// in C++ - drop-in replacements for hand-built Blueprint pack/unpack graphs, one node each.
	// ------------------------------------------------------------------------------------------

	/**
	 * Pack vehicle visualization flags and RandomFraction into a single packed float for use in
	 * Per-Instance Custom Data or Custom Primitive Data.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Vehicle Instance Data")
	static FACFPackedVehicleInstanceCustomData PackVehicleInstanceCustomData(const FACFVehicleInstanceCustomData& CustomData);

	/**
	 * Unpack vehicle visualization flags and RandomFraction from a packed float back out to
	 * discrete fields.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Vehicle Instance Data")
	static FACFVehicleInstanceCustomData UnpackVehicleInstanceCustomData(const FACFPackedVehicleInstanceCustomData& PackedCustomData);

	/**
	 * Reads the packed float directly off a primitive component's live Custom Primitive Data -
	 * no caching, no BP variable.
	 *
	 * @param	PrimitiveComponent	The primitive component to read Custom Primitive Data from.
	 * @param	DataIndex			The CPD index to read. 1 matches MassTraffic's convention.
	 * @param	OutPackedCustomData	Packed custom data if found at DataIndex, otherwise a default
	 *								constructed FACFPackedVehicleInstanceCustomData (PackedParam1 = 0).
	 * @return	true if there was custom primitive data at DataIndex, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "ACF|Vehicle Instance Data")
	static bool GetPackedVehicleInstanceCustomData(const UPrimitiveComponent* PrimitiveComponent, FACFPackedVehicleInstanceCustomData& OutPackedCustomData, int32 DataIndex = 1);
};
