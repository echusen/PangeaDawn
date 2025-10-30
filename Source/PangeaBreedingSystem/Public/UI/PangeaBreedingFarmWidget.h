// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PangeaBreedingFarmWidget.generated.h"

class APangeaEggActor;
class UPangeaBreedableComponent;
class UPangeaBreedingFarmComponent;


/**
 * 
 */
UCLASS()
class PANGEABREEDINGSYSTEM_API UPangeaBreedingFarmWidget : public UUserWidget
{
	GENERATED_BODY()
public:
    UPangeaBreedingFarmWidget(const FObjectInitializer& ObjectInitializer);

    /** The farm this UI represents */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Breeding|References", meta=(ExposeOnSpawn=true))
    TObjectPtr<UPangeaBreedingFarmComponent> CurrentFarm;

    /** Currently selected male creature */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Breeding|Selection")
    TObjectPtr<UPangeaBreedableComponent> SelectedMale;

    /** Currently selected female creature */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Breeding|Selection")
    TObjectPtr<UPangeaBreedableComponent> SelectedFemale;

    /** Called when the player initiates a breeding attempt */
    UFUNCTION(BlueprintCallable, Category="Breeding|UI")
    void TryBreedUI();

    /** Adds an egg to the UI list (designer overrides this in Blueprint) */
    UFUNCTION(BlueprintImplementableEvent, Category="Breeding|UI")
    void AddEggEntry(APangeaEggActor* Egg);

    /** Clears UI selection (designer override optional) */
    UFUNCTION(BlueprintImplementableEvent, Category="Breeding|UI")
    void ClearSelection();

protected:
    virtual void NativeConstruct() override;

    /** Callback for when an egg is spawned from the farm */
    UFUNCTION()
    void OnEggSpawned(APangeaEggActor* NewEgg);

    /** Validates that both selected parents are breedable and belong to the same species */
    bool ValidateParentPair(UPangeaBreedableComponent* Male,UPangeaBreedableComponent* Female, FString& OutReason) const;
};
