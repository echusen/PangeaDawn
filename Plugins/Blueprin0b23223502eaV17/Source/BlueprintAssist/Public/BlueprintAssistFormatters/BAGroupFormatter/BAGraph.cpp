// Copyright fpwong. All Rights Reserved.

#include "BAGraph.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintAssistCache.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "K2Node_Knot.h"
#include "BlueprintAssistMisc/BAGraphSchema.h"
#include "EdGraph/EdGraph.h"

// ======================================
// PinLink
// ======================================

EEdGraphPinDirection FBAPinLink::GetDirection() const
{
	if (DirectionOverride != EGPD_MAX)
	{
		return DirectionOverride;
	}

	// TODO this should return the graph default direction
	return From.IsValid() ? From->Direction : EGPD_Output;
}

FBANodePtr FBAPinLink::GetNode() const
{
	return To.IsValid() ? To->Node : FallbackNode;
}

FBANodePtr FBAPinLink::GetFromNode() const
{
	return From.IsValid() ? From->Node : FallbackNode;
}

FBANodePtr FBAPinLink::GetToNode() const
{
	return To.IsValid() ? To->Node : FallbackNode;
}

FString FBAPinLink::ToString() const
{
	if (FallbackNode)
	{
		return FString::Printf(
			TEXT("Fallback Link %s %s"),
			*GetNameSafe(FallbackNode),
			*(GetDirection() == EGPD_Input ? FString("<-") : FString("->"))
		);
	}

	if (!From.IsValid() && !To.IsValid())
	{
		return FString("EMPTY LINK");
	}

	return FString::Printf(
		TEXT("%s[%s] %s %s[%s]"),
		*GetNameSafe(GetDirection() == EGPD_Output ? GetFromNode() : GetToNode()),
		*GetNameSafe(GetDirection() == EGPD_Output ? From : To),
		*(GetDirection() == EGPD_Output ? FString("->") : FString("<-")),
		*GetNameSafe(GetDirection() == EGPD_Output ? GetToNode() : GetFromNode()),
		*GetNameSafe(GetDirection() == EGPD_Output ? To : From)
	);
}

FSlateRect FBAPinLink::GetBounds() const
{
	const auto FromPos = From->GetPos();
	const auto ToPos = To->GetPos();

	const float Left = FMath::Min(FromPos.X, ToPos.X);
	const float Right = FMath::Max(FromPos.X, ToPos.X);

	const float Top = FMath::Min(FromPos.Y, ToPos.Y);
	const float Bottom = FMath::Max(FromPos.Y, ToPos.Y);

	return FSlateRect(FVector2D(Left, Top), FVector2D(Right, Bottom));
}

bool FBAPinLink::IsExec(const FBAPinLink& Link)
{
	return Link.From ? Link.From->IsExecPin() : false;
}

// ======================================
// Pin
// ======================================

FBAPin::FBAPin(FBANodePtr InNode, UEdGraphPin* Pin)
{
	Node = InNode;
	Direction = Pin->Direction;
	PinHandle.SetPin(Pin);
	PinName = FBAUtils::GetPinName(Pin, false);

	if (const float* Offset = InNode->Graph->GraphHandler->GetPinOffset(PinHandle.GetPin()))
	{
		CachedPinOffset = *Offset;
	}
}

TArray<FBAPinLink> FBAPin::GetLinks()
{
	TArray<FBAPinLink> PinLinks;
	PinLinks.Reserve(LinkedTo.Num());
	for (FBAPinPtr Pin : LinkedTo)
	{
		PinLinks.Add(FBAPinLink(AsShared(), Pin));
	}

	return PinLinks;
}

FBAPinPtr FBAPin::GetPreviousPin()
{
	TArray<FBAPinPtr> Pins = Node->GetPins(Direction);
	return Pins.IsValidIndex(PinIndex - 1) ? Pins[PinIndex - 1] : nullptr;
}

FBAPinPtr FBAPin::GetNextPin()
{
	TArray<FBAPinPtr> Pins = Node->GetPins(Direction);
	return Pins.IsValidIndex(PinIndex + 1) ? Pins[PinIndex + 1] : nullptr;
}

FBAPinPtr FBAPin::GetPreviousLinkedPin()
{
	FBAPinPtr Prev = GetPreviousPin();
	while (Prev)
	{
		if (Prev->LinkedTo.Num())
		{
			return Prev;
		}

		Prev = Prev->GetPreviousPin();
	}

	return nullptr;
}

FVector2D FBAPin::GetPos()
{
	const FSlateRect NodeBounds = Node->GetBounds();

	// use node left and right for the pin pos x
	return FVector2D(Direction == EGPD_Input ? NodeBounds.Left : NodeBounds.Right, Node->NodePosY + CachedPinOffset);
}

UEdGraphPin* FBAPin::GetObj()
{
	return PinHandle.GetPin();
}

bool FBAPin::IsExecPin()
{
	return Node->Graph->GetSchema().IsExecPin(PinHandle.GetPin());
}

FString FBAPin::GetName(bool bIncludeNodeName)
{
	if (this == nullptr)
	{
		return FString("nullptr");
	}

	if (bIncludeNodeName)
	{
		return FString::Printf(TEXT("%s[%s]"), *Node->GetName(), *PinName);
	}

	return PinName;
}

// ======================================
// Node
// ======================================

FBANode::FBANode(FBAGraphPtr InGraph, UEdGraphNode* InNode)
{
	Graph = InGraph;
	NodeObj = InNode;
	NodePosX = NodeObj->NodePosX;
	NodePosY = NodeObj->NodePosY;
}

void FBANode::InitPins()
{
	for (UEdGraphPin* Pin : NodeObj->Pins)
	{
		Pins.Add(MakeShared<FBAPin>(AsShared(), Pin));
	}

	// sort pins and assign index
	Pins.StableSort([](const FBAPinPtr& A, const FBAPinPtr& B)
	{
		return A->CachedPinOffset < B->CachedPinOffset;
	});

	for (EEdGraphPinDirection Dir : {EGPD_Output, EGPD_Input})
	{
		TArray<FBAPinPtr> PinsInDir = GetPins(Dir);
		for (int i = 0; i < PinsInDir.Num(); ++i)
		{
			PinsInDir[i]->PinIndex = i; 
		}
	}
}

bool FBANode::IsExecNode()
{
	return Pins.Num() && Pins.ContainsByPredicate(&FBAPin::IsExecPin);
}

FSlateRect FBANode::GetBounds()
{
	TSharedPtr<FBAGraphHandler> GH = Graph->GraphHandler;

	FVector2D Size(300, 150);
	if (FBAUtils::IsKnotNode(NodeObj))
	{
		Size = FBAUtils::GetKnotNodeSize();
	}
	else
	{
		FBANodeData& FoundNodeData = GH->GetNodeData(NodeObj);
		if (FoundNodeData.HasSize())
		{
			// @TODO comment bubble size?
			Size.X = FoundNodeData.Size.X;
			Size.Y = FoundNodeData.Size.Y;
		}
		else
		{
			if (TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GH->GetGraphPanel(), NodeObj))
			{
				Size = GraphNode->GetDesiredSize();
			}
		}
	}

	return FSlateRect::FromPointAndExtent(GetPos(), Size);
}

FString FBANode::GetName()
{
	if (this == nullptr)
	{
		return FString("nullptr");
	}

	return FBAUtils::GetNodeName(NodeObj);
}

TArray<FBAPinPtr> FBANode::GetPins(EEdGraphPinDirection Direction)
{
	TArray<FBAPinPtr> Out;
	for (FBAPinPtr Pin : Pins)
	{
		if (Pin->Direction == Direction || Direction == EGPD_MAX)
		{
			Out.Add(Pin);
		}
	}

	return Out;
}

TArray<FBAPinLink> FBANode::GetLinks(EEdGraphPinDirection Direction)
{
	TArray<FBAPinLink> Out;
	for (FBAPinPtr Pin : GetPins())
	{
		if (Pin->Direction == Direction || Direction == EGPD_MAX)
		{
			Out.Append(Pin->GetLinks());
		}
	}
	return Out;
}

// ======================================
// Graph
// ======================================

TSharedRef<FBAGraph> FBAGraph::MakeGraph(TSharedPtr<FBAGraphHandler> InGraphHandler, UEdGraphNode* InitialNode)
{
	TSharedRef<FBAGraph> OutGraph = MakeShared<FBAGraph>();

	TArray<UEdGraphNode*> NodeObjTree = FBAUtils::GetNodeTree(InitialNode).Array();
	OutGraph->MakeGraph(InGraphHandler, NodeObjTree);

	return OutGraph;
}

void FBAGraph::MakeGraph(TSharedPtr<FBAGraphHandler> InGraphHandler, const TArray<UEdGraphNode*>& EdGraphNodes)
{
	GraphHandler = InGraphHandler;

	TArray<UEdGraphNode*> NonKnotNodes = EdGraphNodes.FilterByPredicate([](UEdGraphNode* Node)
	{
		return !FBAUtils::IsKnotNode(Node);
	});

	NodeTree = TSet(MoveTemp(NonKnotNodes));

	for (UEdGraphNode* Node : NodeTree)
	{
		TSharedRef<FBANode> NewNode = MakeShared<FBANode>(AsShared(), Node);
		NewNode->InitPins();
		Nodes.Add(NewNode);

		NodeMapping.Add(Node, NewNode.ToSharedPtr());
	}

	InitConnections();
}

void FBAGraph::PrintGraph()
{
	UE_LOG(LogTemp, Warning, TEXT("Graph"));
	for (auto& Node : Nodes)
	{
		UE_LOG(LogTemp, Warning, TEXT("\tNODE %s"), *FBAUtils::GetNodeName(Node->NodeObj));
		for (auto& Pin : Node->Pins)
		{
			if (Pin->LinkedTo.Num())
			{
				UE_LOG(LogTemp, Warning, TEXT("\t\tPIN %s"), *FBAUtils::GetPinName(Pin->PinHandle.GetPin()));
			}
		}
	}
}

void FBAGraph::ApplyGraph(TOptional<FText> Transaction)
{
	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();

	if (Transaction)
	{
		FScopedTransaction ScopedTransaction(Transaction.GetValue());

		for (const FBANodePtr& Node : Nodes)
		{
			Graph->GetSchema()->SetNodePosition(Node->NodeObj, Node->GetPos());
		}
	}
	else // this is dumb...
	{
		for (const FBANodePtr& Node : Nodes)
		{
			Graph->GetSchema()->SetNodePosition(Node->NodeObj, Node->GetPos());
		}
	}
}

const UBAGraphSchema& FBAGraph::GetSchema() const
{
	return GraphHandler->GetSchema();
}

TArray<UEdGraphNode*> FBAGraph::GetNodeObjs() const
{
	TArray<UEdGraphNode*> Out;
	Out.Reserve(Nodes.Num());
	Algo::Transform(Nodes, Out, &FBANode::NodeObj);
	return Out;
}

void FBAGraph::InitConnections()
{
	TMap<UEdGraphPin*, FBAPinPtr> PinMap;

	for (auto& Node : Nodes)
	{
		for (auto& Pin : Node->Pins)
		{
			// UE_LOG(LogTemp, Warning, TEXT("PIN %p %s"), Pin->PinHandle.GetPin(), *Pin->PinHandle.PinId.ToString());
			PinMap.Add(Pin->PinHandle.GetPin(), Pin);
		}
	}

	for (auto& Node : Nodes)
	{
		for (auto& Pin : Node->Pins)
		{
			UEdGraphPin* GraphPin = Pin->PinHandle.GetPin();
			check(GraphPin);

			for (UEdGraphPin* LinkedGraphPin : FBAUtils::GetPinLinkedToIgnoringKnots(GraphPin))
			{
				if (!NodeTree.Contains(LinkedGraphPin->GetOwningNode()))
				{
					continue;
				}

				if (FBAPinPtr LinkedPin = PinMap.FindRef(LinkedGraphPin))
				{
					Pin->LinkedTo.Add(LinkedPin);
				}
				else
				{
					check(false);
				}
			}
		}
	}
}
