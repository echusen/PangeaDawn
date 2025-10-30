// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "AMSTypes.h"

bool FAMSMarker::ValidCheck() const
{
    return markerWidget && IsValid(markerComp);
}