// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ANSNavPopUpWidget.h"

void UANSNavPopUpWidget::ResetFocusOnOwner()
{
    if (pageOwner) {
        pageOwner->SetNavigationEnabled(true);
        if (pageOwner->GetFocusedWidget()) {
            pageOwner->SetFocusToWidget(pageOwner->GetFocusedWidget());
        }
    }
}

void UANSNavPopUpWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (pageOwner) {
        pageOwner->SetNavigationEnabled(false);
    }
}
