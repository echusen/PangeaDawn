// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Items/ACFQuestItem.h"

UACFQuestItem::UACFQuestItem()
{

    ItemInfo.ItemType = EItemType::Quest;
    ItemInfo.Name = FText::FromString("Quest Item");
    ItemInfo.bDroppable = false;
    ItemInfo.bSellable = false;
    ItemInfo.ItemWeight = 0.f;
}
