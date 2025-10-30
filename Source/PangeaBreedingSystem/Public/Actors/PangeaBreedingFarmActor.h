// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PangeaBreedingFarmActor.generated.h"

class UBoxComponent;
class UPangeaBreedingFarmComponent;

UCLASS()
class PANGEABREEDINGSYSTEM_API APangeaBreedingFarmActor : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    APangeaBreedingFarmActor();

    /** Root scene to move the farm in the editor */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USceneComponent> Root;

    /** Editable collision box to define the breeding zone */
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    TObjectPtr<UBoxComponent> BreedingZone;

    /** The logic component that manages breeding */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UPangeaBreedingFarmComponent> FarmLogic;

    /** Visual debug color for the zone (optional) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visualization")
    FColor ZoneColor = FColor(0, 255, 255, 64);

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
};