// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/ACFFirstPersonItemTypes.h"
#include "ACFFirstPersonItemInterface.generated.h"

class UACFProceduralPresetData;
class USceneComponent;

UINTERFACE(BlueprintType)
class ASCENTFIRSTPERSON_API UACFFirstPersonItemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by anything the character can hold, so the first person component can read how it behaves in the
 * hands without knowing the item system it comes from.
 *
 * Everything item specific lives in the item's preset data, each setting in the preset of the motion it drives:
 * aim points in the aim preset, the collision point in the blocking preset, fire rate in the recoil preset. An item
 * whose data lacks one of those presets has neither that motion nor its settings.
 */
class ASCENTFIRSTPERSON_API IACFFirstPersonItemInterface
{
	GENERATED_BODY()

public:
	/**
	 * True while the item cannot describe itself yet, e.g. before its definition reaches a client. The first
	 * person component runs on its defaults meanwhile and asks again every frame.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ACF|First Person")
	bool IsFirstPersonItemPending() const;

	/** Handed to every motion component while the item is held. None falls back to the component's DefaultPresetData. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ACF|First Person")
	UACFProceduralPresetData* GetFirstPersonPresetData() const;

	/** Component the item's points are resolved on when they name no other one. Usually the item mesh. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ACF|First Person")
	USceneComponent* GetFirstPersonItemMesh() const;

	/**
	 * Bone the item is held by, such as hand_l for a bow: the weapon holder copies it and the item's points are
	 * measured from it. None keeps the first person component's ItemReferenceBone.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ACF|First Person")
	FName GetFirstPersonItemReferenceBone() const;

	/** Collision point to use when the blocking preset sets none, such as a ranged weapon's muzzle. False for none. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ACF|First Person")
	bool GetFirstPersonDefaultCollisionPoint(FACFFirstPersonItemPoint& OutPoint) const;
};
