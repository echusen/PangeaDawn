// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ACFItemFragment.h"
#include "Presets/ACFProceduralPreset.h"

#include "ACFFirstPersonFragment.generated.h"

/**
 * Item fragment that gives an item its first person behaviour: the procedural preset data the character's motion
 * components run on while it is the main weapon.
 *
 * Everything item specific lives in that data, each setting in the preset of the motion it drives: aim points in
 * the aim preset, the collision point in the blocking preset, fire rate and fire mode in the recoil preset. A melee
 * weapon's data simply has no aim or recoil preset, so it has no such settings either.
 *
 * Data only: the equipment component hands the main weapon to the UACFFirstPersonComponent, which reads this
 * through the item actor. Items without it run on that component's DefaultPresetData.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "First Person Fragment"))
class INVENTORYSYSTEM_API UACFFirstPersonFragment : public UACFItemFragment {
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "ACF|First Person")
	UACFProceduralPresetData* GetPresetData() const { return PresetData; }

	UFUNCTION(BlueprintPure, Category = "ACF|First Person")
	FName GetItemReferenceBone() const { return ItemReferenceBone; }

protected:
	/** None runs the item on the first person component's DefaultPresetData. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|First Person")
	TObjectPtr<UACFProceduralPresetData> PresetData;

	/**
	 * Bone this item is held by, such as hand_l for a bow, where most items keep the first person component's
	 * ItemReferenceBone (hand_r) by leaving this None. The item's points are measured from it, and the Bone Rebase
	 * node copies it onto the weapon holder through its Pivot Source Bone Override pin.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|First Person")
	FName ItemReferenceBone;
};
