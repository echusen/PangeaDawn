// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "AUTTypes.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "AUTUIFunctionLibrary.generated.h"

class USoundClass;

/**
 *
 */
UCLASS()
class ASCENTUITOOLS_API UAUTUIFunctionLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = AUT)
    static class UAUTGameUserSettings* GetGameUserSettings();

    UFUNCTION(BlueprintCallable, Category = AUT)
    static bool TryGetButtonStyle(int32 styleIndex, FAUTButtonStyle& outStyle);

    UFUNCTION(BlueprintCallable, Category = AUT)
    static bool TryGetSliderStyle(int32 styleIndex, FAUTSliderStyle& outStyle);

    UFUNCTION(BlueprintCallable, Category = AUT)
    static bool TryGetComboBoxStyle(int32 styleIndex, FAUTComboBoxStyle& outStyle);

    UFUNCTION(BlueprintCallable, Category = AUT)
    static bool TryGetCheckBoxStyle(int32 styleIndex, FAUTCheckBoxStyle& outStyle);

    UFUNCTION(BlueprintCallable, Category = AUT)
    static bool TryGetTitleStyle(int32 styleIndex, FAUTBaseTextStyle& outStyle);

    UFUNCTION(BlueprintCallable, Category = AUT)
    static bool TryGetBackgroundStyle(int32 styleIndex, FSlateBrush& outStyle);

    UFUNCTION(BlueprintCallable, Category = AUT)
    static bool TryGetSpacerStyle(int32 styleIndex, FSlateBrush& outStyle);

    UFUNCTION(BlueprintPure, Category = AUT)
    static TSoftObjectPtr<UWorld> GetDefaultMenuLevel();

    UFUNCTION(BlueprintPure, Category = AUT)
    static TSoftObjectPtr<UWorld> GetDefaultNewGameLevel();

    UFUNCTION(BlueprintPure, Category = AUT)
    static TArray<TSoftObjectPtr<USoundClass>> GetDefaultSoundClasses();

    UFUNCTION(BlueprintCallable, Category = AUT)
    static void SetSoundClassVolume(USoundClass* TargetClass, float NewVolume);
};
