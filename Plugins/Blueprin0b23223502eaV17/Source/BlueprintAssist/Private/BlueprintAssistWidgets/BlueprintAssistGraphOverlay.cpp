#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"

#include "BlueprintAssistCache.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistSettings_EditorFeatures.h"
#include "BlueprintAssistWidgets/SBASizeProgress.h"
#include "BlueprintAssistStyle.h"
#include "BlueprintAssistUtils.h"
#include "SGraphPanel.h"
#include "BAGraphHandler/BAGraphOperation.h"
#include "EdGraph/EdGraph.h"

void SBlueprintAssistGraphOverlay::Construct(const FArguments& InArgs, TSharedPtr<FBAGraphHandler> InOwnerGraphHandler)
{
	OwnerGraphHandler = InOwnerGraphHandler;
	SetVisibility(EVisibility::HitTestInvisible);

	CachedBorderBrush = FBAStyle::GetBrush("BlueprintAssist.WhiteBorder");
	CachedLockBrush = FBAStyle::GetPluginBrush("BlueprintAssist.Lock");

	SetCanTick(true);

	AddSlot().VAlign(VAlign_Fill).HAlign(HAlign_Fill).Padding(0)
	[
		SAssignNew(SizeProgressWidget, SBASizeProgress, OwnerGraphHandler)
	];
}

int32 SBlueprintAssistGraphOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!SizeProgressWidget->IsSnapshotValid())
	{
		SizeProgressWidget->HideOverlay();
	}

	const int32 OutgoingLayer = SOverlay::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// don't paint anything when the size progress is visible
	if (SizeProgressWidget->bIsVisible)
	{
		return OutgoingLayer;
	}

	// do nothing if the current graph handler is not our owner graph handler (or is null)
	TSharedPtr<FBAGraphHandler> CurrentGraphHandler = FBAUtils::GetCurrentGraphHandler();
	if (!CurrentGraphHandler || CurrentGraphHandler != OwnerGraphHandler)
	{
		return OutgoingLayer;
	}

	TSharedPtr<SGraphPanel> GraphPanel = CurrentGraphHandler->GetGraphPanel();
	if (!GraphPanel)
	{
		return OutgoingLayer;
	}

	// highlight pins
	if (GraphPanel->GetCurrentLOD() > EGraphRenderingLOD::Type::LowestDetail)
	{
		for (auto Kvp : PinsToHighlight)
		{
			FBAGraphPinHandle PinHandle = Kvp.Key;
			FLinearColor Color = Kvp.Value;

			if (UEdGraphPin* Pin = PinHandle.GetPin(false))
			{
				if (TSharedPtr<SGraphPin> GraphPin = FBAUtils::GetGraphPin(GraphPanel, Pin))
				{
					if (FBAUtils::IsPinVisible(GraphPin))
					{
						const FSlateRect PinBounds = FBAUtils::GetPinBounds(GraphPin);
						if (GraphPanel->IsRectVisible(PinBounds.GetBottomRight(), PinBounds.GetTopLeft()))
						{
							const FPaintGeometry PaintGeometry = GraphPin->GetPaintSpaceGeometry().ToPaintGeometry();

							// Draw a border around the pin
							FSlateDrawElement::MakeBox(
								OutDrawElements,
								OutgoingLayer,
								PaintGeometry,
								CachedBorderBrush,
								ESlateDrawEffect::None,
								Color
							);
						}
					}
				}
			}
		}
	}

	if (CurrentNodeToDraw.IsSet())
	{
		const FBAGraphDrawState& State = CurrentNodeToDraw.GetValue();
		if (TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GraphPanel, State.Node.Get()))
		{
			const FSlateRect NodeBounds = FBAUtils::GetNodeBounds(GraphNode);
			if (GraphPanel->IsRectVisible(NodeBounds.GetBottomRight(), NodeBounds.GetTopLeft()))
			{
				const FPaintGeometry PaintGeometry = GraphNode->GetPaintSpaceGeometry().ToPaintGeometry();

				// Draw a border around the pin
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					OutgoingLayer,
					PaintGeometry,
					CachedBorderBrush,
					ESlateDrawEffect::None,
					State.Color
				);

				if (!State.Text.IsEmpty())
				{
					const FSlateFontInfo FontInfo = FSlateFontInfo(FCoreStyle::GetDefaultFont(), 40);
					DrawTextOverWidget(OutDrawElements, OutgoingLayer + 1, GraphPanel, GraphNode, NodeBounds, State.Text, FontInfo, FLinearColor::Black);
				}
			}
		}

		UEdGraphPin* PinA = State.PinA.GetPinConst();
		UEdGraphPin* PinB = State.PinB.GetPinConst();
		if (PinA && PinB)
		{
			auto PosA = FBAUtils::GetPinPos(CurrentGraphHandler, PinA);
			auto PosB = FBAUtils::GetPinPos(CurrentGraphHandler, PinB);
			const FVector2D Start = FBAUtils::GraphCoordToPanelCoord(GraphPanel, PosA);
			const FVector2D End = FBAUtils::GraphCoordToPanelCoord(GraphPanel, PosB);
			constexpr int LineWidth = 10.0f;
			constexpr float ArrowSize = 60.0f;
			constexpr float ArrowAngle = 30.0f;

			TArray<FVector2D> LinePoints = { Start, End };
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				State.Color,
				true,
				LineWidth); // Thinned the line slightly for better look

			const FVector2D Direction = (End - Start).GetSafeNormal();
			if (!Direction.IsNearlyZero())
			{
				// Calculate the wings by rotating the direction vector
				FVector2D LeftWing = End - Direction.GetRotated(ArrowAngle) * ArrowSize;
				FVector2D RightWing = End - Direction.GetRotated(-ArrowAngle) * ArrowSize;

				// 3. Draw the Arrow Head (as a line strip)
				TArray<FVector2D> ArrowPoints = { LeftWing, End, RightWing };
				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId + 1,
					AllottedGeometry.ToPaintGeometry(),
					ArrowPoints,
					ESlateDrawEffect::None,
					State.Color,
					true,
					LineWidth);
			}
		}
	}

	// draw lines
	for (const FBAGraphOverlayLineParams& ToDraw : LinesToDraw)
	{
		const FVector2D Start = FBAUtils::GraphCoordToPanelCoord(GraphPanel, ToDraw.Start);
		const FVector2D End = FBAUtils::GraphCoordToPanelCoord(GraphPanel, ToDraw.End);
		TArray<FVector2D> LinePoints = { Start, End };

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			ToDraw.Color,
			true,
			5.0f);
	}

	// draw bounds
	for (const FBAGraphOverlayBounds& ToDraw : BoundsToDraw)
	{
		if (GraphPanel->IsRectVisible(ToDraw.Bounds.GetBottomRight(), ToDraw.Bounds.GetTopLeft()))
		{
			const FVector2D TL = FBAUtils::GraphCoordToPanelCoord(GraphPanel, ToDraw.Bounds.GetTopLeft());
			const FVector2D TR = FBAUtils::GraphCoordToPanelCoord(GraphPanel, ToDraw.Bounds.GetTopRight());
			const FVector2D BL = FBAUtils::GraphCoordToPanelCoord(GraphPanel, ToDraw.Bounds.GetBottomLeft());
			const FVector2D BR = FBAUtils::GraphCoordToPanelCoord(GraphPanel, ToDraw.Bounds.GetBottomRight());
			TArray<FVector2D> LinePoints = { TL, TR, BR, BL, TL };

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				ToDraw.Color,
				true,
				2.0f);
		}
	}

	DrawNodeGroups(OutDrawElements, AllottedGeometry, OutgoingLayer, GraphPanel);

	// draw locked nodes
	for (auto Node : CurrentGraphHandler->GetFocusedEdGraph()->Nodes)
	{
		if (CurrentGraphHandler->GetNodeData(Node).bLocked)
		{
			TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GraphPanel, Node);
			if (GraphNode)
			{
				const static FVector2D ImageSize = FVector2D(16, 16);
				DrawIconOnNode(OutDrawElements, OutgoingLayer, GraphNode, GraphPanel, CachedLockBrush, ImageSize, FVector2D(0, 1.0f));
			}
		}
	}

	for (auto& Elem : TextToDraw)
	{
		const FBAGraphOverlayTextParams& Param = Elem.Value;
		if (Param.Widget.IsValid())
		{
			const FSlateFontInfo FontInfo = FSlateFontInfo(FCoreStyle::GetDefaultFont(), 11);
			DrawWidgetAsBox(OutDrawElements, OutgoingLayer - 1, GraphPanel, Param.Widget.Pin(), Param.WidgetBounds, FLinearColor::Black);
			DrawTextOverWidget(OutDrawElements, OutgoingLayer + 1, GraphPanel, Param.Widget.Pin(), Param.WidgetBounds, Param.Text, FontInfo);
		}
	}

	FBADrawContext Context(AllottedGeometry, OutDrawElements, LayerId, GraphPanel, CurrentGraphHandler);

	for (auto It = DebugDrawActions.CreateConstIterator(); It; ++It)
	{
		if (TSharedPtr<IBADrawAction> Action = It->Value)
		{
			Action->Draw(Context);
		}
	}

	if (TSharedPtr<FBAGraphOperation> Operation = CurrentGraphHandler->GetCurrentOperation())
	{
		Operation->OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	return OutgoingLayer;
}

void SBlueprintAssistGraphOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	for (auto It = DebugDrawActions.CreateIterator(); It; ++It)
	{
		TSharedPtr<IBADrawAction> Action = It->Value;
		if (!Action || !Action->Update(InDeltaTime))
		{
			It.RemoveCurrent();
		}
	}

	for (int i = LinesToDraw.Num() - 1; i >= 0; --i)
	{
		LinesToDraw[i].TimeRemaining -= InDeltaTime;
		if (LinesToDraw[i].TimeRemaining <= 0)
		{
			LinesToDraw.RemoveAt(i);
		}
	}

	for (int i = BoundsToDraw.Num() - 1; i >= 0; --i)
	{
		BoundsToDraw[i].TimeRemaining -= InDeltaTime;
		if (BoundsToDraw[i].TimeRemaining <= 0)
		{
			BoundsToDraw.RemoveAt(i);
		}
	}

	// update text to draw
	TArray<TWeakPtr<SWidget>> Widgets;
	TextToDraw.GetKeys(Widgets);
	for (auto& Key : Widgets)
	{
		FBAGraphOverlayTextParams& Value = TextToDraw[Key];
		if (Value.TimeRemaining > 0.0f)
		{
			Value.TimeRemaining -= InDeltaTime;
			if (Value.TimeRemaining <= 0.0f)
			{
				TextToDraw.Remove(Key);
			}
		}
	}

	if (QueueTimer <= 0)
	{
		if (!NodeQueueToDraw.IsEmpty())
		{
			FBAGraphDrawState Next;
			while (NodeQueueToDraw.Dequeue(Next))
			{
				if (Next.Node.IsValid())
				{
					CurrentNodeToDraw = Next;
					break;
				}
			}

			QueueTimer += 0.5f;
		}
		else
		{
			CurrentNodeToDraw.Reset();
		}
	}
	else
	{
		QueueTimer -= InDeltaTime;
	}

	// update the pins to highlight map by removing invalid pin handles
	TArray<FBAGraphPinHandle> GraphPinHandles;
	PinsToHighlight.GetKeys(GraphPinHandles);

	for (FBAGraphPinHandle& PinHandle : GraphPinHandles)
	{
		// if the pin we want to highlight is ever null, remove it from the map
		if (PinHandle.GetPin() == nullptr)
		{
			PinsToHighlight.Remove(PinHandle);
		}
	}
}

void SBlueprintAssistGraphOverlay::DrawWidgetAsBox(
	FSlateWindowElementList& OutDrawElements,
	const int32 OutgoingLayer,
	TSharedPtr<SGraphPanel> GraphPanel,
	TSharedPtr<SWidget> Widget,
	const FSlateRect& WidgetBounds,
	const FLinearColor& Color) const
{
	if (!GraphPanel->IsRectVisible(WidgetBounds.GetBottomRight(), WidgetBounds.GetTopLeft()))
	{
		return;
	}

	const FPaintGeometry PaintGeometry = Widget->GetPaintSpaceGeometry().ToPaintGeometry();

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		OutgoingLayer,
		PaintGeometry,
		CachedBorderBrush,
		ESlateDrawEffect::None,
		Color
	);
}

void SBlueprintAssistGraphOverlay::DrawBoundsAsLines(
	FSlateWindowElementList& OutDrawElements,
	const FGeometry& AllottedGeometry,
	const int32 OutgoingLayer,
	TSharedPtr<SGraphPanel> GraphPanel,
	const FSlateRect& Bounds,
	const FLinearColor& Color,
	float LineWidth) const
{
	const FVector2D TL = FBAUtils::GraphCoordToPanelCoord(GraphPanel, Bounds.GetTopLeft());
	const FVector2D TR = FBAUtils::GraphCoordToPanelCoord(GraphPanel, Bounds.GetTopRight());
	const FVector2D BL = FBAUtils::GraphCoordToPanelCoord(GraphPanel, Bounds.GetBottomLeft());
	const FVector2D BR = FBAUtils::GraphCoordToPanelCoord(GraphPanel, Bounds.GetBottomRight());

	if (!GraphPanel->IsRectVisible(Bounds.GetBottomRight(), Bounds.GetTopLeft()))
	{
		return;
	}

	TArray<FVector2D> LinePoints = { TL, TR, BR, BL, TL };

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		OutgoingLayer,
		AllottedGeometry.ToPaintGeometry(),
		LinePoints,
		ESlateDrawEffect::None,
		Color,
		true,
		LineWidth * GraphPanel->GetZoomAmount());
}

void SBlueprintAssistGraphOverlay::DrawIconOnNode(
	FSlateWindowElementList& OutDrawElements,
	const int32 OutgoingLayer,
	TSharedPtr<SGraphNode> GraphNode,
	TSharedPtr<SGraphPanel> GraphPanel,
	const FSlateBrush* IconBrush,
	const FVector2D& IconSize,
	const FVector2D& IconOffset) const
{
	const FSlateRect NodeBounds = FBAUtils::GetNodeBounds(GraphNode);
	const FSlateRect ImageBounds = FSlateRect::FromPointAndExtent(NodeBounds.GetBottomLeft(), IconSize);

	if (GraphPanel->IsRectVisible(ImageBounds.GetBottomRight(), ImageBounds.GetTopLeft()))
	{
		FVector2D Offset = IconSize * -0.5f + NodeBounds.GetSize() * IconOffset;

#if BA_UE_VERSION_OR_LATER(5, 2)
		const FPaintGeometry PaintGeometry = GraphNode->GetPaintSpaceGeometry().ToPaintGeometry(IconSize, FSlateLayoutTransform(Offset)); 
#else
		const FPaintGeometry PaintGeometry = GraphNode->GetPaintSpaceGeometry().ToPaintGeometry(Offset, IconSize);
#endif

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			OutgoingLayer,
			PaintGeometry,
			IconBrush,
			ESlateDrawEffect::None,
			FLinearColor::White
		);
	}
}

void SBlueprintAssistGraphOverlay::DrawNodeGroups(
	FSlateWindowElementList& OutDrawElements,
	const FGeometry& AllottedGeometry,
	const int32 OutgoingLayer,
	TSharedPtr<SGraphPanel> GraphPanel) const
{
	TSharedPtr<FBAGraphHandler> CurrentGraphHandler = FBAUtils::GetCurrentGraphHandler();
	const UBASettings_EditorFeatures* BASettings_Features = GetDefault<UBASettings_EditorFeatures>();

	// draw fill for groups of the selected nodes
	if (BASettings_Features->bDrawNodeGroupFill)
	{
		TSet<FGuid> NodeGroups;
		for (UEdGraphNode* EdGraphNode : CurrentGraphHandler->GetSelectedNodes())
		{
			const FBANodeData& NodeData = CurrentGraphHandler->GetNodeData(EdGraphNode);
			if (NodeData.NodeGroup.IsValid())
			{
				NodeGroups.Add(NodeData.NodeGroup);
			}
		}

		for (const FGuid& Group : NodeGroups)
		{
			for (UEdGraphNode* Node : CurrentGraphHandler->GetNodeGroup(Group))
			{
				if (TSharedPtr<SGraphNode> GraphNode = CurrentGraphHandler->GetGraphNode(Node))
				{
					DrawWidgetAsBox(OutDrawElements, OutgoingLayer, GraphPanel, GraphNode, FBAUtils::GetNodeBounds(GraphNode), BASettings_Features->NodeGroupFillColor);
				}
			}
		}
	}

	if (BASettings_Features->bDrawNodeGroupOutline)
	{
		TSet<FGuid> NodeGroups;
		if (BASettings_Features->bOnlyDrawGroupOutlineWhenSelected)
		{
			for (UEdGraphNode* SelectedNode : CurrentGraphHandler->GetSelectedNodes())
			{
				const FBANodeData& NodeData = CurrentGraphHandler->GetNodeData(SelectedNode);
				if (NodeData.NodeGroup.IsValid())
				{
					NodeGroups.Add(NodeData.NodeGroup);
				}
			}
		}
		else
		{
			CurrentGraphHandler->NodeGroups.GetKeys(NodeGroups);
		}

		for (const FGuid& NodeGroup : NodeGroups)
		{
			TSet<UEdGraphNode*> Nodes = CurrentGraphHandler->GetNodeGroup(NodeGroup);
			if (Nodes.Num() > 0)
			{
				FSlateRect AllBounds = FBAUtils::GetNodeArrayBounds(Nodes.Array()).ExtendBy(BASettings_Features->NodeGroupOutlineMargin);
				DrawBoundsAsLines(OutDrawElements, AllottedGeometry, OutgoingLayer, GraphPanel, AllBounds, BASettings_Features->NodeGroupOutlineColor, BASettings_Features->NodeGroupOutlineWidth);
			}
		}
	}
}

void SBlueprintAssistGraphOverlay::DrawTextOverWidget(
	FSlateWindowElementList& OutDrawElements,
	const int32 OutgoingLayer,
	TSharedPtr<SGraphPanel> GraphPanel,
	TSharedPtr<SWidget> Widget,
	const FSlateRect& WidgetBounds,
	FText Text,
	FSlateFontInfo Font,
	const FLinearColor& Color) const
{
	if (!GraphPanel->IsRectVisible(WidgetBounds.GetBottomRight(), WidgetBounds.GetTopLeft()))
	{
		return;
	}

	const FPaintGeometry PaintGeometry = Widget->GetPaintSpaceGeometry().ToPaintGeometry();

	FSlateDrawElement::MakeText(
		OutDrawElements,
		OutgoingLayer,
		PaintGeometry,
		Text,
		Font,
		ESlateDrawEffect::None,
		Color
	);
}

int32 SBlueprintAssistGraphOverlay::AddDrawAction(TSharedPtr<IBADrawAction> Action, int32 ActionHash)
{
	if (ActionHash == -1)
	{
		ActionHash = GetTypeHash(FGuid::NewGuid());
	}

	DebugDrawActions.Add(ActionHash, Action);
	return ActionHash;
}

int32 SBlueprintAssistGraphOverlay::AddDrawAction(FBADrawBuilder& Builder, int32 ActionHash)
{
	if (TSharedPtr<IBADrawAction> Action = Builder.Get())
	{
		int32 OutHash = AddDrawAction(Builder.Get(), ActionHash);
		return OutHash;
	}

	return -1;
}

bool SBlueprintAssistGraphOverlay::RemoveDrawAction(int32 ActionHash)
{
	return DebugDrawActions.Remove(ActionHash) > 0;
}

TSharedPtr<IBADrawAction> SBlueprintAssistGraphOverlay::GetDrawAction(int32 ActionHash)
{
	return DebugDrawActions.FindRef(ActionHash);
}

void SBlueprintAssistGraphOverlay::AddHighlightedPin(const FBAGraphPinHandle& PinHandle, const FLinearColor& Color)
{
	if (PinHandle.IsValid())
	{
		PinsToHighlight.Add(PinHandle, Color);
	}
}

void SBlueprintAssistGraphOverlay::AddHighlightedPin(UEdGraphPin* Pin, const FLinearColor& Color)
{
	AddHighlightedPin(FBAGraphPinHandle(Pin), Color);
}

void SBlueprintAssistGraphOverlay::RemoveHighlightedPin(const FBAGraphPinHandle& PinHandle)
{
	PinsToHighlight.Remove(PinHandle);
}

void SBlueprintAssistGraphOverlay::RemoveHighlightedPin(UEdGraphPin* Pin)
{
	RemoveHighlightedPin(FBAGraphPinHandle(Pin));
}

void SBlueprintAssistGraphOverlay::DrawDebugLine(const FString& DebugChannel, const FVector2D& Start, const FVector2D& End, FLinearColor Color, float Duration)
{
	if (BA_DEBUG(DebugChannel))
		DrawLine(Start, End, Color, Duration);
}

void SBlueprintAssistGraphOverlay::DrawDebugPinLink(const FString& DebugChannel, const FPinLink& PinLink, FLinearColor Color, float Duration)
{
	if (BA_DEBUG(DebugChannel) && PinLink.From && PinLink.To)
	{
		AddDrawAction(FBADrawBuilder()
			.Add(MakeShared<FBADrawPinLinkAction>(PinLink.From, PinLink.To, Color))
			.Timed(Duration)
		);
	}
}

void SBlueprintAssistGraphOverlay::DrawDebugBounds(const FString& DebugChannel, const FSlateRect& Bounds, FLinearColor Color, float Duration)
{
	if (BA_DEBUG(DebugChannel))
		DrawBounds(Bounds, Color, Duration);
}

void SBlueprintAssistGraphOverlay::DrawNodeInQueue(UEdGraphNode* Node, FLinearColor Color)
{
	FBAGraphDrawState State;
	State.Color = Color;
	State.Node = Node;
	NodeQueueToDraw.Enqueue(State);
}

void SBlueprintAssistGraphOverlay::DrawStateInQueue(const FBAGraphDrawState& State)
{
	NodeQueueToDraw.Enqueue(State);
}

void SBlueprintAssistGraphOverlay::DrawTextOverWidget(const FBAGraphOverlayTextParams& Params)
{
	AddDrawAction(FBADrawBuilder()
	.Add(MakeShared<FBADrawTextOnWidgetAction>(Params.Text, Params.Widget, Params.WidgetBounds, Params.Color))
	.Timed(Params.TimeRemaining));
}

void SBlueprintAssistGraphOverlay::RemoveTextOverWidget(TSharedPtr<SWidget> Widget)
{
	TextToDraw.Remove(Widget);
}
