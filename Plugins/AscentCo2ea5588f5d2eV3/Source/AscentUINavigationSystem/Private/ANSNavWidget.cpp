// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ANSNavWidget.h"
#include "ANSNavPageWidget.h"
#include "ANSUITypes.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"

UANSNavWidget::UANSNavWidget(const FObjectInitializer& UANSNavWidget)
    : Super(UANSNavWidget)
{
    SetIsFocusable(false);
    bIsHovered = false;
}

void UANSNavWidget::RequestFocus()
{
    TryGetPageOwner();
    if (navPage) {
        navPage->SetFocusToWidget(this);
    }
}

void UANSNavWidget::SetIsSelected(bool val)
{
    bIsSelected = val;
    if (IsValidLowLevelFast()) {
        UpdateStyle(bIsHovered || bIsSelected);
    }
}

bool UANSNavWidget::GetFocusedSubWidget(FFocusedWidget& outwidget, UANSNavPageWidget* pageOwner)
{
    SetPageOwner(pageOwner);
    UWidget* subWidg = GetFocusableSubWidget();
    if (subWidg && subWidg->GetIsEnabled() && subWidg->HasAnyUserFocus()) {
        outwidget = FFocusedWidget(subWidg, this);
        return true;
    }
    TArray<UWidget*> navWidgets;
    WidgetTree->GetAllWidgets(navWidgets);
    for (auto widget : navWidgets) {
        if (widget && widget->GetIsEnabled() && widget->HasAnyUserFocus()
            && widget != this && !widget->IsA<UANSNavWidget>()) {
            outwidget = FFocusedWidget(widget, this);
            return true;
        } else if (widget->IsA<UANSNavWidget>()) {
            UANSNavWidget* navWidget = Cast<UANSNavWidget>(widget);
            if (navWidget->GetFocusedSubWidget(outwidget, pageOwner)) {
                return true;
            }
        }
    }
    return false;
}

void UANSNavWidget::OnFocusReceived()
{
    bIsHovered = true;
    if (IsValidLowLevelFast()) {
        UpdateStyle(bIsHovered || bIsSelected);
        HandleFocusReceived();
    }
}

void UANSNavWidget::OnFocusLost()
{
    bIsHovered = false;
    if (IsValidLowLevelFast()) {
        UpdateStyle(bIsHovered || bIsSelected);
        HandleFocusLost();
    }
}

void UANSNavWidget::SetPageOwner(class UANSNavPageWidget* pageOwner)
{
    navPage = pageOwner;
}

void UANSNavWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    UpdateStyle(bIsHovered);
}

void UANSNavWidget::NativeConstruct()
{
    TryGetPageOwner();
    Super::NativeConstruct();
}

void UANSNavWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    if (!bIsHovered) {
        OnFocusReceived();
    }
}

void UANSNavWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    OnFocusLost();
}

void UANSNavWidget::TryGetPageOwner()
{
    if (!navPage) {
        TArray<UUserWidget*> foundWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, foundWidgets, UANSNavPageWidget::StaticClass());
        if (foundWidgets.IsValidIndex(0)) {
            SetPageOwner(Cast<UANSNavPageWidget>(foundWidgets[0]));
        } else {
            UE_LOG(LogTemp, Error, TEXT("Using NavWidgets outside of NavPages! - UANSNavWidget::TryGetPageOwner"));
        }
    }
}

void UANSNavWidget::RequestFocusForWidget(UANSNavWidget* widgetToFocus)
{
    TryGetPageOwner();
    if (navPage) {
        navPage->SetFocusToWidget(widgetToFocus);
    } else {
        UE_LOG(LogTemp, Error, TEXT("Invalid nav Page for NavWidget! - UANSNavWidget::RequestFocusForWidget"));
    }
}
