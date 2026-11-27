#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"

#include "ANSNavPageWidget.h"
#include "AUTUIFunctionLibrary.h"
#include "Widgets/AUTBorder.h"
#include "Widgets/AUTTextBlock.h"

// Private presentation implementation shared by the mining screens. Gameplay and
// transfer requests remain on their existing Blueprint-callable widget functions.
struct FMiningUIPresentation
{
	/** Closes the screen: removes the hosting ANS navigation page when there is one, otherwise the widget itself. */
	static void Close(UUserWidget* Content)
	{
		if (UANSNavPageWidget* Page = Content->GetTypedOuter<UANSNavPageWidget>())
		{
			Page->RemoveSelf();
		}
		else
		{
			Content->RemoveFromParent();
		}
	}

	/** Creates a wrapping text block with the default UI style, or the highlight style for sizes of 22 and above. */
	static UAUTTextBlock* Text(UWidgetTree* Tree, FName Name, int32 Size = 18)
	{
		UAUTTextBlock* const Result = Tree->ConstructWidget<UAUTTextBlock>(UAUTTextBlock::StaticClass(), Name);
		Result->SetStyle(FGameplayTag::RequestGameplayTag(Size >= 22 ? TEXT("UIStyle.Text.Highlight") : TEXT("UIStyle.Text.Default")));
		Result->SetFontSize(Size);
		Result->SetAutoWrapText(true);

		return Result;
	}

	/** Applies the default UI button style, when that style is registered. */
	static void Button(UButton* Button)
	{
		FAUTButtonStyle Style;

		if (UAUTUIFunctionLibrary::TryGetButtonStyle(FGameplayTag::RequestGameplayTag(TEXT("UIStyle.Button.Default")), Style))
		{
			Button->SetStyle(Style.ButtonStyle);
		}
	}

	/** Builds the centered, size-limited window frame as the tree's root and returns the panel that takes the content. */
	static UAUTBorder* Screen(UWidgetTree* Tree)
	{
		UOverlay* const Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ScreenRoot"));
		Tree->RootWidget = Root;
		USizeBox* const Bounds = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScreenBounds"));
		Bounds->SetMaxDesiredWidth(880.f);
		Bounds->SetMaxDesiredHeight(780.f);
		UOverlaySlot* const Slot = Root->AddChildToOverlay(Bounds);
		Slot->SetHorizontalAlignment(HAlign_Center);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(32.f));
		UAUTBorder* const Panel = Tree->ConstructWidget<UAUTBorder>(UAUTBorder::StaticClass(), TEXT("RootBorder"));
		Panel->SetStyle(FGameplayTag::RequestGameplayTag(TEXT("UIStyle.Background.Window")));
		Panel->SetPadding(FMargin(24.f));
		Bounds->SetContent(Panel);

		return Panel;
	}

	/** Wraps Content in a scroll box and makes that the panel's content. */
	static void ScrollContent(UWidgetTree* Tree, UAUTBorder* Panel, UWidget* Content)
	{
		UScrollBox* const Scroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScreenScroll"));
		Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		Panel->SetContent(Scroll);
		Scroll->AddChild(Content);
	}
};
