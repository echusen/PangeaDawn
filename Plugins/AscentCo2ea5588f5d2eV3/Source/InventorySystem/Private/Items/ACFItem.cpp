// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Items/ACFItem.h"
#include "GameFramework/Pawn.h"
#include "ItemActors/ACFItemActor.h"
#include "Net/UnrealNetwork.h"
#include <AbilitySystemComponent.h>
#include <ActiveGameplayEffectHandle.h>
#include <GameplayEffect.h>
#include <GameplayEffectTypes.h>
#include <UObject/UObjectGlobals.h>

// Sets default values
UACFItem::UACFItem()
{

    ItemInfo.ItemType = EItemType::Other;
}

TSubclassOf<AACFItemActor> UACFItem::GetItemActorClass_Implementation() const
{
    return nullptr;
}

UWorld* UACFItem::GetWorld() const
{
    if (ItemOwner) {
        return ItemOwner->GetWorld();
    }
    return nullptr;
}

UACFItemFragment* UACFItem::GetFragmentByClass(TSubclassOf<UACFItemFragment> FragmentClass) const
{
    if (!FragmentClass) {
        return nullptr;
    }

    for (UACFItemFragment* Fragment : Fragments) {
        if (Fragment && Fragment->IsA(FragmentClass)) {
            return Fragment;
        }
    }

    return nullptr;
}

