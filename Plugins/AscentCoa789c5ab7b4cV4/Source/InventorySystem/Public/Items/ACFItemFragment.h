// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include "ACFItemFragment.generated.h"

/*
 * Base class for item fragments used to compose item definitions in ACF Inventory System.
 * Can be extended in Blueprint to add custom data to items.
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class INVENTORYSYSTEM_API UACFItemFragment : public UObject {
    GENERATED_BODY()

public:
    // Default constructor for the item fragment class.
    UACFItemFragment() {};
};