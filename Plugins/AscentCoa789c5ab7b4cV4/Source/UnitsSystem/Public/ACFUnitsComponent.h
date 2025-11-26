// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ACFAITypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "ACFUnitsComponent.generated.h"

class AACFCharacter;
class UACFGroupAIComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitsChanged, const TArray<FBaseUnit>&, newUnits);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitAdded, const FBaseUnit&, newUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnitRemoved, const FBaseUnit&, Unit);


UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class UNITSSYSTEM_API UACFUnitsComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UACFUnitsComponent();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Savegame, BlueprintReadOnly, ReplicatedUsing = OnRepUnits, Category = ACF)
    TArray<FBaseUnit> Units;


public:
    UFUNCTION(BlueprintPure, Category = ACF)
    TArray<FBaseUnit> GetUnits() const
    {
        return Units;
    }

    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnUnitsChanged OnUnitsChanged;

    UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnUnitAdded OnUnitAdded;

       UPROPERTY(BlueprintAssignable, Category = ACF)
    FOnUnitRemoved OnUnitRemoved;

    UFUNCTION(BlueprintCallable, Category = ACF)
    void AddUnit(const TSubclassOf<AACFCharacter>& unit);

    UFUNCTION(BlueprintCallable, Category = ACF)
    bool RemoveUnit(const TSubclassOf<AACFCharacter>& unit);

    UFUNCTION(BlueprintCallable, Category = ACF)
    bool MoveUnitToGroup(const TSubclassOf<AACFCharacter>& unit, UACFGroupAIComponent* groupAI);

    UFUNCTION(BlueprintCallable, Category = ACF)
    bool MoveUnitFromGroup(const TSubclassOf<AACFCharacter>& unit, UACFGroupAIComponent* groupAI);

private:
    UFUNCTION()
    void OnRepUnits();
};
