// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AMSDiscoverComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ASCENTMAPSSYSTEM_API UAMSDiscoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAMSDiscoverComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** World-space radius in centimeters revealed around the owner each tick */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AMS|FogOfWar", meta = (ClampMin = "0.0"))
	float RevealRadius = 2000.f;
};
