// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistWidgets/BADrawAction.h"

#include "BlueprintAssistStyle.h"
#include "BlueprintAssistUtils.h"
#include "SGraphPanel.h"

void FBADrawContext::DrawWidgetAsBox(TSharedPtr<SWidget> Widget, const FSlateRect& WidgetBounds, const FLinearColor& Color) const
{
	if (!GraphPanel->IsRectVisible(WidgetBounds.GetBottomRight(), WidgetBounds.GetTopLeft()))
	{
		return;
	}

	const FPaintGeometry PaintGeometry = Widget->GetPaintSpaceGeometry().ToPaintGeometry();

	static const FSlateBrush* BorderBrush = FBAStyle::GetBrush("BlueprintAssist.WhiteBorder");

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		PaintGeometry,
		BorderBrush,
		ESlateDrawEffect::None,
		Color
	);
}

void FBADrawContext::DrawTextOverWidget(const FText& Text, const FSlateFontInfo& FontInfo, TSharedPtr<SWidget> Widget, const FSlateRect& WidgetBounds, const FLinearColor& Color) const
{
	if (!GraphPanel->IsRectVisible(WidgetBounds.GetBottomRight(), WidgetBounds.GetTopLeft()))
	{
		return;
	}

	const FPaintGeometry PaintGeometry = Widget->GetPaintSpaceGeometry().ToPaintGeometry();

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		PaintGeometry,
		Text,
		FontInfo,
		ESlateDrawEffect::None,
		Color
	);
}

void FBADrawContext::DrawArrow(FVector2D Start, FVector2D End, FLinearColor Color, int LineWidth, float ArrowSize, float ArrowAngle) const
{
	// main line
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		{Start, End},
		ESlateDrawEffect::None,
		Color,
		true,
		LineWidth);

	// arrow
	const FVector2D Direction = (End - Start).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		FVector2D LeftWing = End - Direction.GetRotated(ArrowAngle) * ArrowSize;
		FVector2D RightWing = End - Direction.GetRotated(-ArrowAngle) * ArrowSize;

		TArray<FVector2D> ArrowPoints = {LeftWing, End, RightWing};
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			ArrowPoints,
			ESlateDrawEffect::None,
			Color,
			true,
			LineWidth);
	}
}

void FBADrawLineAction::Draw(const FBADrawContext& Ctx) const
{
	FSlateDrawElement::MakeLines(Ctx.OutDrawElements, Ctx.LayerId, Ctx.AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
}

void FBADrawBoundsAction::Draw(const FBADrawContext& Ctx) const
{
	const auto& Panel = Ctx.GraphPanel;
	if (Panel->IsRectVisible(Bounds.GetBottomRight(), Bounds.GetTopLeft()))
	{
		const FVector2D TL = FBAUtils::GraphCoordToPanelCoord(Panel, Bounds.GetTopLeft());
		const FVector2D TR = FBAUtils::GraphCoordToPanelCoord(Panel, Bounds.GetTopRight());
		const FVector2D BL = FBAUtils::GraphCoordToPanelCoord(Panel, Bounds.GetBottomLeft());
		const FVector2D BR = FBAUtils::GraphCoordToPanelCoord(Panel, Bounds.GetBottomRight());
		TArray<FVector2D> LinePoints = {TL, TR, BR, BL, TL};

		FSlateDrawElement::MakeLines(
			Ctx.OutDrawElements,
			Ctx.LayerId,
			Ctx.AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	}
}

void FBADrawNodeAction::Draw(const FBADrawContext& Ctx) const
{
	if (Node.IsValid())
	{
		if (TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GraphPanel, Node.Get()))
		{
			Ctx.DrawWidgetAsBox(GraphNode, FBAUtils::GetNodeBounds(GraphNode), Color);
		}
	}
}

void FBADrawTextOnWidgetAction::Draw(const FBADrawContext& Ctx) const
{
	if (Widget.IsValid())
	{
		static FSlateFontInfo FontInfo = FSlateFontInfo(FCoreStyle::GetDefaultFont(), 11);
		Ctx.DrawWidgetAsBox(Widget.Pin(), WidgetBounds, FLinearColor::Black);
		Ctx.DrawTextOverWidget(Text, FontInfo, Widget.Pin(), WidgetBounds, Color);
	}
}

void FBADrawPinLinkAction::Draw(const FBADrawContext& Ctx) const
{
	UEdGraphPin* PinA = HandleA.GetPinConst();
	UEdGraphPin* PinB = HandleB.GetPinConst();
	if (PinA && PinB)
	{
		const FVector2D Start = FBAUtils::GraphCoordToPanelCoord(Ctx.GraphPanel, FBAUtils::GetPinPos(Ctx.GraphHandler, PinA));
		const FVector2D End = FBAUtils::GraphCoordToPanelCoord(Ctx.GraphPanel, FBAUtils::GetPinPos(Ctx.GraphHandler, PinB));

		constexpr int LineWidth = 10.0f;
		constexpr float ArrowSize = 60.0f;
		constexpr float ArrowAngle = 30.0f;

		Ctx.DrawArrow(Start, End, Color, LineWidth, ArrowSize, ArrowAngle);
	}
}
