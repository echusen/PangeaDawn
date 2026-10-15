// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Components/Border.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "AUTBorder.generated.h"

/**
 *
 */
UCLASS()
class ASCENTUITOOLS_API UAUTBorder : public UBorder {
    GENERATED_BODY()

protected:
    /** Background style this border asks the current theme for. */
    UPROPERTY(EditAnywhere, Category = AUT, meta = (Categories = "UIStyle.Background"))
    FGameplayTag StyleTag;

    UPROPERTY(EditAnywhere, Category = AUT)
    bool bUseStyle = true;

    virtual void SynchronizeProperties() override;
    virtual void PostLoad() override;

private:
    /** Background this border used to point at by index. Turned into StyleTag on load, never edited again. */
    UPROPERTY()
    int32 StyleIndex = 0;

public:
    /** Applies the background brush the current theme maps to inStyleTag. */
    UFUNCTION(BlueprintCallable, Category = AUT)
    void SetStyle(UPARAM(meta = (Categories = "UIStyle.Background")) FGameplayTag inStyleTag);

    UFUNCTION(BlueprintPure, Category = AUT)
    FGameplayTag GetStyleTag() const
    {
        return StyleTag;
    }
};
