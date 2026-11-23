// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ACFWalkingCollisionProxy.generated.h"

class UACFWalkingCollisionComponent;

UCLASS(NotPlaceable)
class CLIMBINGSYSTEM_API AACFWalkingCollisionProxy : public AActor
{
	GENERATED_BODY()

public:
	AACFWalkingCollisionProxy();

	UACFWalkingCollisionComponent* GetWalkingCollisionComponent() const { return WalkingCollisionComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "ACF")
	TObjectPtr<UACFWalkingCollisionComponent> WalkingCollisionComponent = nullptr;
};
