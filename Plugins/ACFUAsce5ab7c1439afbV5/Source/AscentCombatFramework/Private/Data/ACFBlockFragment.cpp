// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Data/ACFBlockFragment.h"
#include "ACFGASStatisticsComponent.h"
#include "Components/ACFBlockComponent.h"
#include "GameFramework/Pawn.h"
#include "ItemActors/ACFItemActor.h"
#include "Items/ACFItem.h"

UACFBlockFragment::UACFBlockFragment()
{
}

void UACFBlockFragment::ApplyFragment_Implementation(APawn* pawnOwner, UACFItem* itemOwner)
{
	if (!pawnOwner || !itemOwner) {
		return;
	}

	cachedPawn = pawnOwner;
	cachedItem = itemOwner;

	if (bApplyModifierWhileEquipped) {
		UACFGASStatisticsComponent* statComp = pawnOwner->FindComponentByClass<UACFGASStatisticsComponent>();
		if (statComp) {
			equippedModifierHandle = statComp->AddAttributeSetModifier(BlockModifier);
		}
	}

	if (TryConfigureBlockComponent()) {
		return;
	}

	// The equipment component applies fragments before spawning the ItemActor, so the actor to add
	// the block component to still doesn't exist. OnEquipmentChanged closes the equip flow right
	// after the spawn: configure the component from there.
	UACFEquipmentComponent* equipComp = pawnOwner->FindComponentByClass<UACFEquipmentComponent>();
	if (equipComp && !equipComp->OnEquipmentChanged.IsAlreadyBound(this, &UACFBlockFragment::HandleEquipmentChanged)) {
		equipComp->OnEquipmentChanged.AddDynamic(this, &UACFBlockFragment::HandleEquipmentChanged);
		boundEquipComp = equipComp;
	}
}

void UACFBlockFragment::RemoveFragment_Implementation(APawn* pawnOwner, UACFItem* itemOwner)
{
	UnbindFromEquipment();

	if (equippedModifierHandle.IsValid() && pawnOwner) {
		UACFGASStatisticsComponent* statComp = pawnOwner->FindComponentByClass<UACFGASStatisticsComponent>();
		if (statComp) {
			statComp->RemoveAttributeSetModifier(equippedModifierHandle);
		}
	}
	equippedModifierHandle = FActiveGameplayEffectHandle();

	// Components that were already on the ItemActor Blueprint are left alone: the actor itself is
	// destroyed on unequip, so there's nothing to restore.
	if (bCreatedBlockComp && blockComp.IsValid()) {
		blockComp->DestroyComponent();
	}

	blockComp = nullptr;
	bCreatedBlockComp = false;
	cachedPawn = nullptr;
	cachedItem = nullptr;
}

void UACFBlockFragment::ConfigureBlockComponent(UACFBlockComponent* blockComponent)
{
	if (!blockComponent) {
		return;
	}

	blockComponent->SetDefendingModifier(BlockModifier);
	blockComponent->SetBlockDirection(BlockDirection);
	blockComponent->SetMaxBlockAngleDegrees(MaxBlockAngleDegrees);
	blockComponent->SetBlockableDamages(BlockableDamages);
	blockComponent->SetCounterableDamages(CounterableDamages);
	blockComponent->SetCanParry(bCanParry);
	blockComponent->SetDamagedStatisticMultiplier(DamagedStatisticMultiplier);
}

bool UACFBlockFragment::TryConfigureBlockComponent()
{
	AACFItemActor* itemActor = FindOwningItemActor();
	if (!itemActor) {
		return false;
	}

	UACFBlockComponent* itemBlockComp = itemActor->FindComponentByClass<UACFBlockComponent>();

	if (itemBlockComp) {
		if (bOverrideExistingComponent) {
			ConfigureBlockComponent(itemBlockComp);
		}
	} else {
		itemBlockComp = NewObject<UACFBlockComponent>(itemActor,  UACFBlockComponent::StaticClass());
		if (!itemBlockComp) {
			return false;
		}

		// AddInstanceComponent registers the component in the actor's instance component list so
		// editor, serialisation and component iterators all see it correctly. RegisterComponent
		// finalises it and calls BeginPlay if the world has already started.
		itemActor->AddInstanceComponent(itemBlockComp);
		itemBlockComp->RegisterComponent();

		if (itemBlockComp->GetIsReplicated()) {
			itemActor->AddReplicatedSubObject(itemBlockComp);
		}

		bCreatedBlockComp = true;
		ConfigureBlockComponent(itemBlockComp);
	}

	blockComp = itemBlockComp;
	OnBlockComponentConfigured(itemBlockComp, itemActor);
	return true;
}

AACFItemActor* UACFBlockFragment::FindOwningItemActor() const
{
	APawn* pawnOwner = cachedPawn.Get();
	UACFItem* itemOwner = cachedItem.Get();

	if (!pawnOwner || !itemOwner) {
		return nullptr;
	}

	const UACFEquipmentComponent* equipComp = pawnOwner->FindComponentByClass<UACFEquipmentComponent>();
	if (!equipComp) {
		return nullptr;
	}

	const TArray<FEquippedItem> equippedItems = equipComp->GetCurrentEquipment().GetEquippedItems();
	for (const FEquippedItem& equippedItem : equippedItems) {
		if (equippedItem.Item == itemOwner && equippedItem.ItemActor) {
			return equippedItem.ItemActor;
		}
	}

	return nullptr;
}

void UACFBlockFragment::HandleEquipmentChanged(const FEquipment& equipment)
{
	if (TryConfigureBlockComponent()) {
		UnbindFromEquipment();
	}
}

void UACFBlockFragment::UnbindFromEquipment()
{
	if (boundEquipComp.IsValid()) {
		boundEquipComp->OnEquipmentChanged.RemoveDynamic(this, &UACFBlockFragment::HandleEquipmentChanged);
	}
	boundEquipComp = nullptr;
}
