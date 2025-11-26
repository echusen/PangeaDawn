// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.


#include "ACFGASDeveloperSettings.h"
#include <Engine/DataTable.h>

UDataTable* UACFGASDeveloperSettings::GetAttributeKeys() const
{
    return Cast<UDataTable>(SerializableAttributes.LoadSynchronous());
}