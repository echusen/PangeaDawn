// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CommonAnimatedSwitcher.h"
#include "Components/WidgetSwitcher.h"
#include "CoreMinimal.h"
#include "UITag.h"

#include "ANSWidgetSwitcher.generated.h"

struct FUIActionTag;
class UANSUIPlayerSubsystem;
class UHorizontalBox;
/**
 *
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FANSOnIndexChanged, int32, newIndex);

UCLASS()
class ASCENTUINAVIGATIONSYSTEM_API UANSWidgetSwitcher : public UCommonAnimatedSwitcher {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = ANS)
    void ProcessOnKeyDown(const FKeyEvent& InKeyEvent);

    UFUNCTION(BlueprintCallable, Category = ANS)
    void NavigateToNext();

    UFUNCTION(BlueprintCallable, Category = ANS)
    void NavigateToPrevious();

    UFUNCTION(BlueprintCallable, Category = ANS)
    void SetTopBar(UHorizontalBox* topbar);

    UPROPERTY(BlueprintAssignable, Category = ANS)
    FANSOnIndexChanged OnIndexChanged;

    UFUNCTION(BlueprintPure, BlueprintCallable, Category = ANS)
    UWidget* GetCurrentActiveWidget() const;

    virtual void SetActiveWidgetIndex(int32 Index) override;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ANS)
    bool bAllowCircularNavigation = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ANS)
    FUIActionTag PreviousAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ANS)
    FUIActionTag NextAction;

    virtual void HandleSlateActiveIndexChanged(int32 ActiveIndex) override;

    UPROPERTY(BlueprintReadOnly, Category = ANS)
    UHorizontalBox* Topbar;

private:
    UANSUIPlayerSubsystem* GetUISubsystem() const;
};
