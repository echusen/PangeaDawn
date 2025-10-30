// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "ANSUIConfigComponent.generated.h"

class UDataTable;


UCLASS(ClassGroup = (ACF), meta = (BlueprintSpawnableComponent))
class ASCENTUINAVIGATIONSYSTEM_API UANSUIConfigComponent : public UActorComponent {
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UANSUIConfigComponent();

    /** Returns the Icons DataTable by tag */
    UFUNCTION(BlueprintPure, Category = "ANS|UI")
    UDataTable* GetIconsByTag() const;

    /** Returns the DataTable for a specific platform */
    UFUNCTION(BlueprintPure, Category = "ANS|UI")
    UDataTable* GetKeysConfigForPlatform(const FString& Platform) const;

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

    /** Soft reference to icons DataTable (configured via config) */
    UPROPERTY(EditAnywhere, Category = "UI Keys",
        meta = (RequiredAssetDataTags = "RowStructure=/Script/AscentUINavigationSystem.ANSIcons"))
    UDataTable* IconsByTag;

    /** Soft references to key config DataTables per platform (configured via config) */
    UPROPERTY(EditAnywhere, Category = "UI Keys",
        meta = (RequiredAssetDataTags = "RowStructure=/Script/AscentUINavigationSystem.ANSKeysIconConfig"))
    TMap<FString, TSoftObjectPtr<UDataTable>> KeysConfigByPlatform;
};
