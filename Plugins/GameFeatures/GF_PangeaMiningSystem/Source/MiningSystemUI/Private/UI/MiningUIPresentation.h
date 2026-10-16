#pragma once

#include "AUTUIFunctionLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Widgets/AUTBorder.h"
#include "Widgets/AUTTextBlock.h"
#include "ANSNavPageWidget.h"

// Private presentation implementation shared by the mining screens. Gameplay and
// transfer requests remain on their existing Blueprint-callable widget functions.
struct FMiningUIPresentation
{
    static void Close(UUserWidget* Content)
    {
        if (UANSNavPageWidget* Page = Content->GetTypedOuter<UANSNavPageWidget>()) Page->RemoveSelf();
        else Content->RemoveFromParent();
    }

    static UAUTTextBlock* Text(UWidgetTree* Tree, FName Name, int32 Size = 18)
    {
        UAUTTextBlock* Result = Tree->ConstructWidget<UAUTTextBlock>(UAUTTextBlock::StaticClass(), Name);
        Result->SetStyle(FGameplayTag::RequestGameplayTag(Size >= 22 ? TEXT("UIStyle.Text.Highlight") : TEXT("UIStyle.Text.Default")));
        Result->SetFontSize(Size);
        Result->SetAutoWrapText(true);
        return Result;
    }

    static void Button(UButton* Button)
    {
        FAUTButtonStyle Style;
        if (UAUTUIFunctionLibrary::TryGetButtonStyle(FGameplayTag::RequestGameplayTag(TEXT("UIStyle.Button.Default")), Style))
        {
            Button->SetStyle(Style.ButtonStyle);
        }
    }

    static UAUTBorder* Screen(UWidgetTree* Tree)
    {
        UOverlay* Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ScreenRoot"));
        Tree->RootWidget = Root;
        USizeBox* Bounds = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScreenBounds"));
        Bounds->SetMaxDesiredWidth(880.f);
        Bounds->SetMaxDesiredHeight(780.f);
        UOverlaySlot* Slot = Root->AddChildToOverlay(Bounds);
        Slot->SetHorizontalAlignment(HAlign_Center);
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin(32.f));
        UAUTBorder* Panel = Tree->ConstructWidget<UAUTBorder>(UAUTBorder::StaticClass(), TEXT("RootBorder"));
        Panel->SetStyle(FGameplayTag::RequestGameplayTag(TEXT("UIStyle.Background.Window")));
        Panel->SetPadding(FMargin(24.f));
        Bounds->SetContent(Panel);
        return Panel;
    }

    static void ScrollContent(UWidgetTree* Tree, UAUTBorder* Panel, UWidget* Content)
    {
        UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScreenScroll"));
        Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
        Panel->SetContent(Scroll);
        Scroll->AddChild(Content);
    }
};
