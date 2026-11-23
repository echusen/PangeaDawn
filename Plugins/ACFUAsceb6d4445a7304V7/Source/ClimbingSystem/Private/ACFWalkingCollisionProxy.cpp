// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFWalkingCollisionProxy.h"

#include "ACFWalkingCollisionComponent.h"

AACFWalkingCollisionProxy::AACFWalkingCollisionProxy()
{
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetReplicateMovement(false);

	WalkingCollisionComponent = CreateDefaultSubobject<UACFWalkingCollisionComponent>(TEXT("WalkingCollisionComponent"));
	SetRootComponent(WalkingCollisionComponent);
}
