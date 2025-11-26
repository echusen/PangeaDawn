// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "Components/ProgressBar.h"
#include "CoreMinimal.h"
#include <Blueprint/UserWidget.h>
#include <AttributeSet.h>
#include <AbilitySystemComponent.h>
#include <GameplayEffectTypes.h>

#include "ACFGASProgressBar.generated.h"

class UAbilitySystemComponent;
class UProgressBar;

/**
 *
 */
UCLASS()
class ASCENTGASRUNTIME_API UACFGASProgressBar : public UUserWidget {
    GENERATED_BODY()

protected:
    /*Sets the attribute used to display Current Value for the bar*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    FGameplayAttribute CurrentAttribute;

    /*Sets the attribute used to display Max Value for the bar*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    FGameplayAttribute MaxAttribute;

    /*Sets the attribute used to display Max Value for the bar*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    FLinearColor FillColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    bool bSmoothInterpolation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ACF)
    float InterpolationSpeed = 3.f;

    UPROPERTY(meta = (BindWidget), EditAnywhere, BlueprintReadWrite, Category = ACF)
    UProgressBar* ACFProgressBar;

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
    UFUNCTION(BlueprintPure, Category = ACF)
    FGameplayAttribute GetCurrentAttribute() const { return CurrentAttribute; }

    UFUNCTION(BlueprintPure, Category = ACF)
    FGameplayAttribute GetMaxAttribute() const { return MaxAttribute; }

    /*Needed to initialize the Progress Bar's component reference*/
    UFUNCTION(BlueprintCallable, Category = ACF)
    void InitBar(UAbilitySystemComponent* inAbilityComp);

	UACFGASProgressBar(const FObjectInitializer& ObjectInitializer);

private:
    TObjectPtr<UAbilitySystemComponent> abilityComp;

    void OnValueChanged(const FOnAttributeChangeData& Data);

    void UpdateBar();

    float targetPercentage;
};
