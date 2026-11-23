// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "ARSTypes.h"
#include "Components/ACFEquipmentComponent.h"
#include "CoreMinimal.h"
#include "Game/ACFTypes.h"
#include "GameFramework/DamageType.h"
#include "Items/ACFItemFragment.h"
#include <ActiveGameplayEffectHandle.h>

#include "ACFBlockFragment.generated.h"

class UACFBlockComponent;
class AACFItemActor;

/**
 * Item fragment that turns the item it belongs to (shield, weapon, ...) into a blockable one.
 *
 * On equip it looks up the ItemActor spawned for the owning item and makes sure it owns a
 * UACFBlockComponent - the component the UACFDefenseStanceComponent looks for through
 * TryGetBestBlockComp() - creating it when the actor Blueprint doesn't already have one, and
 * pushing the block configuration (BlockModifier included) into it.
 *
 * Note that ACFEquipmentComponent applies fragments BEFORE spawning the ItemActor, so when the
 * actor is not there yet the fragment waits for the OnEquipmentChanged broadcast that closes the
 * equip flow and configures the component then.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class ASCENTCOMBATFRAMEWORK_API UACFBlockFragment : public UACFItemFragment {
	GENERATED_BODY()

public:
	UACFBlockFragment();

	virtual void ApplyFragment_Implementation(APawn* pawnOwner, UACFItem* itemOwner) override;
	virtual void RemoveFragment_Implementation(APawn* pawnOwner, UACFItem* itemOwner) override;

	/** The block component currently driven by this fragment, if any */
	UFUNCTION(BlueprintPure, Category = "ACF|Block")
	UACFBlockComponent* GetBlockComponent() const { return blockComp.Get(); }

	/** Modifier applied to the owner while he's defending with this item */
	UFUNCTION(BlueprintPure, Category = "ACF|Block")
	FAttributesSetModifier GetBlockModifier() const { return BlockModifier; }

protected:
	/*Modifier applied to the owner while he's in defense stance with this item.
	It is stored in the BlockComponent as DefendingModifier and applied / removed by
	the UACFDefenseStanceComponent when the block starts / stops*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Block")
	FAttributesSetModifier BlockModifier;

	/*Directions in which incoming hits are blocked*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Block")
	EActionDirection BlockDirection = EActionDirection::Front;

	/*The maximum angle (in degrees) within which the block is considered successful*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "BlockDirection == EActionDirection::Front"), Category = "ACF|Block")
	float MaxBlockAngleDegrees = 90.f;

	/*Types of damages that can be blocked with this item*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Block")
	TArray<TSubclassOf<UDamageType>> BlockableDamages;

	/*Types of damages that can be countered with this item*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Block")
	TArray<TSubclassOf<UDamageType>> CounterableDamages;

	/*If this item can be used to parry*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Block")
	bool bCanParry = false;

	/*Multiplier applied to the damaged attribute (usually stamina) at every blocked hit*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Block")
	float DamagedStatisticMultiplier = 1.f;

	/*If true, the block configuration of this fragment is pushed even on a BlockComponent that was
	already present on the ItemActor. If false, an existing component is left untouched*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Block")
	bool bOverrideExistingComponent = true;

	/*If true the BlockModifier is ALSO applied to the owner for the whole time the item is
	equipped (legacy behaviour), on top of being applied by the defense stance while blocking*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ACF|Block")
	bool bApplyModifierWhileEquipped = false;

	/*Called right after the block component has been retrieved / created and configured,
	to add item specific setup in Blueprint*/
	UFUNCTION(BlueprintNativeEvent, Category = "ACF|Block")
	void OnBlockComponentConfigured(UACFBlockComponent* blockComponent, AACFItemActor* itemActor);
	virtual void OnBlockComponentConfigured_Implementation(UACFBlockComponent* blockComponent, AACFItemActor* itemActor) {}

	/*Copies the fragment block configuration into the provided component*/
	virtual void ConfigureBlockComponent(UACFBlockComponent* blockComponent);

private:
	/** Tries to configure the block component on the ItemActor of the owning item, returns true on success */
	bool TryConfigureBlockComponent();

	/** Finds the actor spawned for the owning item in the equipment of the owning pawn */
	AACFItemActor* FindOwningItemActor() const;

	UFUNCTION()
	void HandleEquipmentChanged(const FEquipment& equipment);

	void UnbindFromEquipment();

	TWeakObjectPtr<APawn> cachedPawn;
	TWeakObjectPtr<UACFItem> cachedItem;
	TWeakObjectPtr<UACFEquipmentComponent> boundEquipComp;
	TWeakObjectPtr<UACFBlockComponent> blockComp;

	/** True when the block component has been created by this fragment and has to be destroyed on unequip */
	bool bCreatedBlockComp = false;

	FActiveGameplayEffectHandle equippedModifierHandle;
};
