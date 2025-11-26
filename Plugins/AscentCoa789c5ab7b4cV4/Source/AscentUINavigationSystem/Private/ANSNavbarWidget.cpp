// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ANSNavbarWidget.h"
#include "ANSNavbarFunctionLibrary.h"
#include "ANSUIPlayerSubsystem.h"
#include "CommonInputSubsystem.h"
#include "Input/CommonUIInputSettings.h"
#include "Kismet/GameplayStatics.h"
#include <Engine/GameInstance.h>
#include <Kismet/GameplayStatics.h>

FReply UANSNavbarWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    ProcessOnKeyDown(InKeyEvent);
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UANSNavbarWidget::UANSNavbarWidget(const FObjectInitializer& ObjectInitializer)
{
    SetIsFocusable(true);
}

ECommonInputType UANSNavbarWidget::GetCurrentInputType() const
{
    return GetInputSubsystem()->GetCurrentInputType();
}

void UANSNavbarWidget::ProcessOnKeyDown(const FKeyEvent& InKeyEvent)
{
    const ECommonInputType localInput = UANSNavbarFunctionLibrary::GetInputTypeByKeyEvent(InKeyEvent);

    if (localInput != InputType) {
        InputType = localInput;
        BuildNavbar();
    }

    TArray<FUIActionTag> actionNames;

    const UGameInstance* gameInst = UGameplayStatics::GetGameInstance(this);
    UANSUIPlayerSubsystem* UISub = gameInst->GetSubsystem<UANSUIPlayerSubsystem>();

    if (UISub->TryGetActionsFromKey(InKeyEvent.GetKey(), actionNames)) {
        for (const FUIActionTag& actionName : actionNames) {
            if (UIActions.Contains(actionName)) {
                OnButtonPressed(actionName);
                break;
            }
        }
    }
}

void UANSNavbarWidget::ProcessOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
}

void UANSNavbarWidget::AddNavbarAction(FUIActionTag action)
{
    if (!UIActions.Contains(action)) {
        // Reordering the array
        const TArray<FUIActionTag> actionsCopy = UIActions;
        UIActions.Empty();
        UIActions.Add(action);
        for (const FUIActionTag& newAction : actionsCopy) {
            UIActions.Add(newAction);
        }

        BuildNavbar();
    }
}

void UANSNavbarWidget::RemoveNavbarAction(FUIActionTag action)
{
    if (UIActions.Contains(action)) {
        UIActions.Remove(action);
        BuildNavbar();
    }
}

void UANSNavbarWidget::SetNavbarActions(const TArray<FUIActionTag>& action)
{
    if (UIActions != action) {
        UIActions = action;
        BuildNavbar();
    }
}

void UANSNavbarWidget::HandleInputChanged(ECommonInputType bNewInputType)
{
    InputType = bNewInputType;
    BuildNavbar();
}

void UANSNavbarWidget::NativeConstruct()
{
    Super::NativeConstruct();
    const UGameInstance* gameInst = UGameplayStatics::GetGameInstance(this);
    UANSUIPlayerSubsystem* UISubsystem = gameInst->GetSubsystem<UANSUIPlayerSubsystem>();
    ActivateWidget();
    SetIsFocusable(true);

    UCommonInputSubsystem* commonInputSub = GetInputSubsystem();
    commonInputSub->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputChanged);
    if (UISubsystem) {
        InputType = commonInputSub->GetCurrentInputType();
        HandleInputChanged(InputType);
        BuildNavbar();
    } else {
        UE_LOG(LogTemp, Error, TEXT("Remember to add a Navbar component to your player controller! - UANSNavbarWidget::NativeConstruct "));
    }
}

void UANSNavbarWidget::NativeDestruct()
{
    UCommonInputSubsystem* commonInputSub = GetInputSubsystem();
    if (commonInputSub) {
        commonInputSub->OnInputMethodChangedNative.RemoveAll(this);
    }
    Super::NativeDestruct();
}

UCommonInputSubsystem* UANSNavbarWidget::GetInputSubsystem() const
{
    // In the new system, we may be representing an action for any player, not necessarily the one that technically owns this action icon widget
    // We want to be sure to use the LocalPlayer that the binding is actually for so we can display the icon that corresponds to their current input method
    const ULocalPlayer* BindingOwner = GetOwningLocalPlayer();
    return UCommonInputSubsystem::Get(BindingOwner);
}

void UANSNavbarWidget::BuildNavbar()
{
    TArray<FANSActionConfig> navActions;
    for (const auto& action : UIActions) {
        FANSActionConfig actionConf;
        const UGameInstance* gameInst = UGameplayStatics::GetGameInstance(this);
        UANSUIPlayerSubsystem* UISub = gameInst->GetSubsystem<UANSUIPlayerSubsystem>();
        if (UISub->TryGetActionConfig(action, InputType, actionConf)) {
            navActions.Add(actionConf);
        }
    }
    FillNavbar(navActions);
}
