// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "AGSCondition.h"
#include "AGSGraphNode.h"

bool UAGSCondition::Verify(class APlayerController* playerController)
{
    Controller = playerController;
    return VerifyCondition(Controller);
}

bool UAGSCondition::VerifyForNode(class APlayerController* playerController, UAGSGraphNode* inNodeOwner)
{
    nodeOwner = inNodeOwner;
    return Verify(playerController);
}
