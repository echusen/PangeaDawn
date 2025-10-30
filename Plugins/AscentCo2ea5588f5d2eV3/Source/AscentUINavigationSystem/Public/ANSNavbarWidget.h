// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ANSUITypes.h"
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UITag.h"
#include <GameplayTagContainer.h>
#include <Input/Events.h>
#include <Layout/Geometry.h>
#include <CommonActivatableWidget.h>

#include "ANSNavbarWidget.generated.h"

class UCommonInputSubsystem;

/**
 *
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FANSOnActionTriggered, const FUIActionTag&, actionName);

UCLASS()
class ASCENTUINAVIGATIONSYSTEM_API UANSNavbarWidget : public UCommonActivatableWidget {
    GENERATED_BODY()

protected:
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;


	UCommonInputSubsystem* GetInputSubsystem() const;
    /*Reinit the navbar*/
    UFUNCTION(BlueprintCallable, Category = ANS)
    void BuildNavbar();

    UFUNCTION(BlueprintCallable, Category = ANS)
    void DispatchOnActionTriggered(const FUIActionTag& action)
    {
        OnActionTriggered.Broadcast(action);
    }


    UFUNCTION(BlueprintImplementableEvent, Category = ANS)
    void FillNavbar(const TArray<FANSActionConfig>& actionConfigs);

    UFUNCTION(BlueprintImplementableEvent, Category = ANS)
    void OnButtonPressed(const FUIActionTag& action);

public:

    UANSNavbarWidget(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ANS)
    TArray<FUIActionTag> UIActions;

    UPROPERTY(BlueprintAssignable, Category = ANS)
    FANSOnActionTriggered OnActionTriggered;

    UFUNCTION(BlueprintPure, Category = ANS)
    ECommonInputType GetCurrentInputType() const;

    UFUNCTION(BlueprintCallable, Category = ANS)
    void ProcessOnKeyDown(const FKeyEvent& InKeyEvent);

    void ProcessOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent);

    UFUNCTION(BlueprintCallable, Category = ANS)
    void AddNavbarAction(FUIActionTag action);

    UFUNCTION(BlueprintCallable, Category = ANS)
    void RemoveNavbarAction(FUIActionTag action);

    /*Override current navbar actions*/
    UFUNCTION(BlueprintCallable, Category = ANS)
    void SetNavbarActions(const TArray<FUIActionTag>& action);

private:
    ECommonInputType InputType;

    UFUNCTION()
    void HandleInputChanged(ECommonInputType bNewInputType);
};
