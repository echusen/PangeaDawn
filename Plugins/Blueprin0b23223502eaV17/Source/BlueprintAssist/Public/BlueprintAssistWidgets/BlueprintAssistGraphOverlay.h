// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BADrawAction.h"
#include "BlueprintAssistTypes.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"

class SBASizeProgress;
class FBAGraphHandler;

struct FBAGraphDrawState
{
	FLinearColor Color = FLinearColor::Green;
	TWeakObjectPtr<UEdGraphNode> Node;
	FBAGraphPinHandle PinA;
	FBAGraphPinHandle PinB;
	FText Text;
};

struct FBAGraphOverlayLineParams
{
	float TimeRemaining = 5.0f;
	FVector2D Start;
	FVector2D End;
	FLinearColor Color = FLinearColor::White;
	TWeakPtr<SGraphPin> StartWidget; 
	TWeakPtr<SGraphPin> EndWidget; 
};

struct FBAGraphOverlayTextParams
{
	FLinearColor Color = FLinearColor::White;
	FText Text;
	FSlateRect WidgetBounds;
	TWeakPtr<SWidget> Widget;
	float TimeRemaining = -1.0f;
};

struct FBAGraphOverlayBounds
{
	float TimeRemaining = 5.0f;
	FSlateRect Bounds;
	FLinearColor Color = FLinearColor::White;
};

class BLUEPRINTASSIST_API SBlueprintAssistGraphOverlay : public SOverlay
{
	SLATE_BEGIN_ARGS(SBlueprintAssistGraphOverlay) {}
	SLATE_END_ARGS()

	TMap<int32, TSharedPtr<IBADrawAction>> DebugDrawActions;

	void Construct(const FArguments& InArgs, TSharedPtr<FBAGraphHandler> InOwnerGraphHandler);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void AddHighlightedPin(const FBAGraphPinHandle& PinHandle, const FLinearColor& Color);
	void AddHighlightedPin(UEdGraphPin* Pin, const FLinearColor& Color);

	void RemoveHighlightedPin(const FBAGraphPinHandle& PinHandle);
	void RemoveHighlightedPin(UEdGraphPin* Pin);

	void DrawLine(const FBAGraphOverlayLineParams& Params)
	{
		LinesToDraw.Add(Params);
	}

	void DrawLine(const FVector2D& Start, const FVector2D& End, FLinearColor Color = FLinearColor::White, float Duration = 3.0f)
	{
		FBAGraphOverlayLineParams Params;
		Params.Start = Start;
		Params.End = End;
		Params.TimeRemaining = Duration;
		Params.Color = Color;
		LinesToDraw.Add(Params);
	}

	void DrawDebugLine(const FString& DebugChannel, const FVector2D& Start, const FVector2D& End, FLinearColor Color = FLinearColor::White, float Duration = 3.0f);

	void DrawDebugPinLink(const FString& DebugChannel, const FPinLink& PinLink, FLinearColor Color = FLinearColor::White, float Duration = 3.0f);

	void DrawBounds(const FSlateRect& Bounds, FLinearColor Color = FLinearColor::Green, float Duration = 3.0f)
	{
		AddDrawAction(FBADrawBuilder()
			.Add(MakeShared<FBADrawBoundsAction>(Bounds, Color))
			.Timed(Duration)
		);
	}

	void DrawDebugBounds(const FString& DebugChannel, const FSlateRect& Bounds, FLinearColor Color = FLinearColor::Green, float Duration = 3.0f);

	void ClearBounds() { BoundsToDraw.Reset(); }

	void DrawBounds(const FBAGraphOverlayBounds& Params)
	{
		BoundsToDraw.Add(Params);
	}

	void DrawNodeInQueue(UEdGraphNode* Node, FLinearColor Color = FLinearColor::Yellow);

	void DrawStateInQueue(const FBAGraphDrawState& State);

	void ClearNodesInQueue() { NodeQueueToDraw.Empty(); }

	TSharedPtr<SBASizeProgress> SizeProgressWidget;

	void DrawTextOverWidget(const FBAGraphOverlayTextParams& Params);
	void RemoveTextOverWidget(TSharedPtr<SWidget> Widget);
	void ClearAllTextOverWidgets() { TextToDraw.Empty(); }
	bool IsDrawingTextOverWidgets() const { return TextToDraw.Num() > 0; }

	int32 AddDrawAction(TSharedPtr<IBADrawAction> Action, int32 ActionHash = -1);
	int32 AddDrawAction(FBADrawBuilder& Builder, int32 ActionHash = -1);
	bool HasDrawAction(int32 ActionHash) const { return DebugDrawActions.Contains(ActionHash); }
	TSharedPtr<IBADrawAction> GetDrawAction(int32 ActionHash);
	bool RemoveDrawAction(int32 ActionHash);

protected:
	TSharedPtr<FBAGraphHandler> OwnerGraphHandler;
	TMap<FBAGraphPinHandle, FLinearColor> PinsToHighlight;

	TArray<FBAGraphOverlayLineParams> LinesToDraw;
	TArray<FBAGraphOverlayBounds> BoundsToDraw;

	TOptional<FBAGraphDrawState> CurrentNodeToDraw;
	TQueue<FBAGraphDrawState> NodeQueueToDraw;
	float QueueTimer = 0.5f;

	TMap<TWeakPtr<SWidget>, FBAGraphOverlayTextParams> TextToDraw;

	const FSlateBrush* CachedBorderBrush = nullptr;
	const FSlateBrush* CachedLockBrush = nullptr;

	void DrawWidgetAsBox(
		FSlateWindowElementList& OutDrawElements,
		const int32 OutgoingLayer,
		TSharedPtr<SGraphPanel> GraphPanel,
		TSharedPtr<SWidget> Widget,
		const FSlateRect& WidgetBounds,
		const FLinearColor& Color = FLinearColor::White) const;

	void DrawBoundsAsLines(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& AllottedGeometry,
		const int32 OutgoingLayer, 
		TSharedPtr<SGraphPanel> GraphPanel,
		const FSlateRect& Bounds,
		const FLinearColor& Color = FLinearColor::White,
		float LineWidth = 1.0f) const;

	/* Icon starts top left, offset should be 0-1 in each axis */
	void DrawIconOnNode(
		FSlateWindowElementList& OutDrawElements,
		const int32 OutgoingLayer,
		TSharedPtr<SGraphNode> GraphNode,
		TSharedPtr<SGraphPanel> GraphPanel,
		const FSlateBrush* IconBrush,
		const FVector2D& IconSize,
		const FVector2D& IconOffset) const;

	void DrawNodeGroups(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& AllottedGeometry,
		const int32 OutgoingLayer,
		TSharedPtr<SGraphPanel> GraphPanel) const;

	void DrawTextOverWidget(
		FSlateWindowElementList& OutDrawElements,
		const int32 OutgoingLayer,
		TSharedPtr<SGraphPanel> GraphPanel,
		TSharedPtr<SWidget> Widget,
		const FSlateRect& WidgetBounds,
		FText Text,
		FSlateFontInfo Font,
		const FLinearColor& Color = FLinearColor::White) const;
};
