// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "ACFSurfaceAssetFactory.generated.h"

UCLASS()
class CLIMBINGSYSTEMEDITOR_API UACFSurfaceAssetFactory : public UFactory
{
	GENERATED_BODY()
	
public:
    UACFSurfaceAssetFactory();

    virtual UObject* FactoryCreateNew(
        UClass* Class,
        UObject* InParent,
        FName Name,
        EObjectFlags Flags,
        UObject* Context,
        FFeedbackContext* Warn) override;
};
