// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Items/ACFEquippableItem.h"
#include "ARSFunctionLibrary.h"
#include "ARSStatisticsComponent.h"
#include <GameFramework/Character.h>
#include <Kismet/GameplayStatics.h>

UACFEquippableItem::UACFEquippableItem()
{
}


void UACFEquippableItem::Internal_OnEquipped(ACharacter* charOwner)
{
    if (charOwner) {
        SetItemOwner(charOwner);
        AddModifierToOwner(AttributeModifier);
        OnItemEquipped();
    } else {
        UE_LOG(LogTemp, Error, TEXT("Invalid owner -AACFEquippableItem "));
    }
}

void UACFEquippableItem::RemoveModifierToOwner(const FActiveGameplayEffectHandle& inModifier)
{
    if (ItemOwner) {
        UARSStatisticsComponent* statcomp = ItemOwner->FindComponentByClass<UARSStatisticsComponent>();
        if (statcomp) {
            statcomp->RemoveAttributeSetModifier(inModifier);
        }
    }
}

void UACFEquippableItem::AddModifierToOwner(const FAttributesSetModifier& inModifier)
{
    if (ItemOwner) {
        UARSStatisticsComponent* statcomp = ItemOwner->FindComponentByClass<UARSStatisticsComponent>();
        if (statcomp) {
            ModifierHandle = statcomp->AddAttributeSetModifier(inModifier);
        }
    }
}

void UACFEquippableItem::RemoveCurrentModifierFromOwner()
{
    RemoveModifierToOwner(ModifierHandle);
}

void UACFEquippableItem::Internal_OnUnEquipped()
{
    RemoveCurrentModifierFromOwner();

    OnItemUnEquipped();
}


void UACFEquippableItem::OnItemEquipped_Implementation()
{
}

void UACFEquippableItem::OnItemUnEquipped_Implementation()
{
}

bool UACFEquippableItem::CanBeEquipped_Implementation(class UACFEquipmentComponent* equipComp)
{
    return true;
}
