// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "Game/ACFDamageType.h"
#include "ACFActionsFunctionLibrary.h"

bool UACFDamageType::bUsingGAS() const
{
    return UARSFunctionLibrary::IsUsingGAS();
}

UACFDamageType::UACFDamageType()
{
    StaggerMutliplier = 1.f;
    
}
