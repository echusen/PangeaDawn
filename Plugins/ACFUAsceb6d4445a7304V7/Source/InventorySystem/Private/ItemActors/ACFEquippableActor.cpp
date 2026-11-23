// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.


#include "ItemActors/ACFEquippableActor.h"

#include "Components/MeshComponent.h"
#include "Items/ACFFirstPersonFragment.h"

namespace ACFEquippableActor
{
    static const UACFFirstPersonFragment* FindFirstPersonFragment(const UACFEquippableItem* equippable)
    {
        return equippable ? Cast<UACFFirstPersonFragment>(equippable->GetFragmentByClass(UACFFirstPersonFragment::StaticClass())) : nullptr;
    }
}


 AACFEquippableActor::AACFEquippableActor()
{
}

bool AACFEquippableActor::IsFirstPersonItemPending_Implementation() const
{
    // Clients build the definition in FEquippedItem::Init, which can land after the actor replicated.
    return GetEquippableDefinition() == nullptr;
}

UACFProceduralPresetData* AACFEquippableActor::GetFirstPersonPresetData_Implementation() const
{
    const UACFFirstPersonFragment* fragment = ACFEquippableActor::FindFirstPersonFragment(GetEquippableDefinition());
    return fragment ? fragment->GetPresetData() : nullptr;
}

FName AACFEquippableActor::GetFirstPersonItemReferenceBone_Implementation() const
{
    const UACFFirstPersonFragment* fragment = ACFEquippableActor::FindFirstPersonFragment(GetEquippableDefinition());
    return fragment ? fragment->GetItemReferenceBone() : NAME_None;
}

USceneComponent* AACFEquippableActor::GetFirstPersonItemMesh_Implementation() const
{
    return FindComponentByClass<UMeshComponent>();
}
