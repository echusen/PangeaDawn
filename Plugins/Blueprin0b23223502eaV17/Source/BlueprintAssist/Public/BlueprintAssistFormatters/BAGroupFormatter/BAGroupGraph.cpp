// Copyright fpwong. All Rights Reserved.

#include "BAGroupGraph.h"

#include "BATree.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintAssistCache.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "EdGraphNode_Comment.h"
#include "BlueprintAssistFormatters/BlueprintAssistCommentContainsGraph.h"
#include "EdGraph/EdGraph.h"

DEFINE_LOG_CATEGORY_STATIC(LogBAGroupGraph, NoLogging, All);

// ======================================
// PinLink
// ======================================

EEdGraphPinDirection FBAGroupLink::GetDirection() const
{
	if (DirectionOverride != EGPD_MAX)
	{
		return DirectionOverride;
	}

	// TODO this should return the graph default direction
	return From.IsValid() ? From->Direction : EGPD_Output;
}

TFunction<bool(const FBAGroupLink&)> FBAGroupLink::MatchesDirection(EEdGraphPinDirection Dir)
{
	return [Dir](const FBAGroupLink& Link)
	{
		return Link.GetDirection() == Dir;
	};
}

FBAGroupNodePtr FBAGroupLink::GetNode() const
{
	return To.IsValid() ? To->Node : FallbackNode;
}

FBAGroupNodePtr FBAGroupLink::GetFromNode() const
{
	return From.IsValid() ? From->Node : FallbackNode;
}

FBAGroupNodePtr FBAGroupLink::GetToNode() const
{
	return To.IsValid() ? To->Node : FallbackNode;
}

FString FBAGroupLink::ToString() const
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

FSlateRect FBAGroupLink::GetBounds() const
{
	const auto FromPos = From->GetPos();
	const auto ToPos = To->GetPos();

	const float Left = FMath::Min(FromPos.X, ToPos.X);
	const float Right = FMath::Max(FromPos.X, ToPos.X);

	const float Top = FMath::Min(FromPos.Y, ToPos.Y);
	const float Bottom = FMath::Max(FromPos.Y, ToPos.Y);

	return FSlateRect(FVector2D(Left, Top), FVector2D(Right, Bottom));
}

FPinLink FBAGroupLink::AsRegularPinLink()
{
	UEdGraphNode* FallbackObj = FallbackNode ? FallbackNode->Nodes[0]->NodeObj : nullptr;
	return FPinLink(From->PinHandle->PinHandle.GetPin(), To->PinHandle->PinHandle.GetPin(), FallbackObj);
}

FBAPinLink FBAGroupLink::AsBAPinLink()
{
	if (FallbackNode)
	{
		return FBAPinLink(FallbackNode->Nodes[0], DirectionOverride);
	}

	return FBAPinLink(From->PinHandle, To->PinHandle);
}

bool FBAGroupLink::IsExecLink()
{
	return !From || From->IsExecPin();
}

bool FBAGroupLink::IsParamLink()
{
	return !From || From->IsParameterPin();
}

float FBAGroupLink::CalcStraightenDist() const
{
	if (!From || !To)
	{
		return 0.0f;
	}

	return From->GetPos().Y - To->GetPos().Y;
}

void FBAGroupLink::Straighten()
{
	if (!From || !To)
	{
		return;
	}

	const float DeltaY = CalcStraightenDist();
	FBAGroupNodePtr ToNode = To->GetOwningNode();
	ToNode->SetY(FMath::RoundToInt32(ToNode->GetY() + DeltaY));
}

bool FBAGroupLink::IsOnSameBranch() const
{
	return HasBothLinks() && GetFromNode()->GetBranch() == GetToNode()->GetBranch(); 
}

bool FBAGroupLink::IsOnSameRootBranch() const
{
	return HasBothLinks() && GetFromNode()->GetRootBranch() == GetToNode()->GetRootBranch();
}

// ======================================
// Pin
// ======================================

FBAGroupPin::FBAGroupPin(FBAGroupNodePtr InNode, FBAPinPtr Pin, int InPinIndex)
{
	Node = InNode;
	Direction = Pin->Direction;
	PinHandle = Pin;
	PinIndex = InPinIndex;

	if (const float* Offset = InNode->Graph->GraphHandler->GetPinOffset(Pin->PinHandle.GetPin()))
	{
		FBAVector2 NodeObjPos = Node->GetContainedNodePos(Pin->GetOwningNode());
		float GraphPinPos = NodeObjPos.Y + *Offset; 
		CachedPinOffset = GraphPinPos - Node->GetY();
	}
}

TArray<FBAGroupLink> FBAGroupPin::GetLinks()
{
	TArray<FBAGroupLink> PinLinks;
	PinLinks.Reserve(LinkedTo.Num());
	for (FBAGroupPinPtr Pin : LinkedTo)
	{
		PinLinks.Add(FBAGroupLink(AsShared(), Pin));
	}

	return PinLinks;
}

FBAGroupPinPtr FBAGroupPin::GetPreviousPin()
{
	TArray<FBAGroupPinPtr> Pins = GetOwningNode()->GetPins(Direction);
	return Pins.IsValidIndex(PinIndex - 1) ? Pins[PinIndex - 1] : nullptr;
}

FBAGroupPinPtr FBAGroupPin::GetNextPin()
{
	TArray<FBAGroupPinPtr> Pins = GetOwningNode()->GetPins(Direction);
	return Pins.IsValidIndex(PinIndex + 1) ? Pins[PinIndex + 1] : nullptr;
}

FVector2D FBAGroupPin::GetPos()
{
	const FSlateRect NodeBounds = Node->GetBounds();

	// use node left and right for the pin pos x
	return FVector2D(Direction == EGPD_Input ? NodeBounds.Left : NodeBounds.Right, Node->GetY() + CachedPinOffset);
}

bool FBAGroupPin::IsParameterPin()
{
	return PinHandle->IsParameterPin();
}

bool FBAGroupPin::IsExecPin()
{
	return PinHandle->IsExecPin();
}

FString FBAGroupPin::GetName()
{
	if (this == nullptr)
	{
		return FString("nullptr");
	}

	return GetNameSafe(PinHandle);
}

// ======================================
// Node
// ======================================

FBAGroupNode::FBAGroupNode(FBAGroupGraphPtr InGraph, const TArray<FBANodePtr>& InNodes, const FSlateRect& InBounds, const FString& InNodeName)
{
	Graph = InGraph;
	Nodes = InNodes;

	NodePosX = InBounds.GetTopLeft().X;
	NodePosY = InBounds.GetTopLeft().Y;
	Size = InBounds.GetSize();

	NodeName = InNodeName;

	for (auto Node : Nodes)
	{
		FBAVector2 Offset(Node->NodePosX - InBounds.Left, Node->NodePosY - InBounds.Top);
		NodeOffsets.Add(Node, Offset);
	}
}

void FBAGroupNode::InitPins()
{
	// create pins if they connect outside of our node set
	for (FBANodePtr Node : Nodes)
	{
		for (EEdGraphPinDirection Dir : {EGPD_Output, EGPD_Input})
		{
			int PinIndex = 0;
			for (FBAPinPtr Pin : Node->GetPins(Dir))
			{
				if (Graph->LinkFilter && !Graph->LinkFilter(*Pin.Get()))
				{
					continue;
				}

				bool bCreatePin = Pin->LinkedTo.ContainsByPredicate([&](FBAPinPtr LinkedTo)
				{
					return !Nodes.Contains(LinkedTo->GetOwningNode());
				});

				if (bCreatePin)
				{
					Pins.Add(MakeShared<FBAGroupPin>(AsShared(), Pin, PinIndex));
				}

				PinIndex += 1;
			}
		}
	}
}

void FBAGroupNode::SetParentLink(const FBAGroupLink& NewParentLink)
{
	// clear our current parent
	FBAGroupNodePtr Parent = ParentLink.GetFromNode();
	if (Parent.IsValid())
	{
		Parent->Children.Remove(ParentLink);
	}

	// add ourself to the next parent
	if (FBAGroupNodePtr NewParent = NewParentLink.GetFromNode())
	{
		NewParent->Children.Add(NewParentLink);
	}

	// store our parent link
	ParentLink = NewParentLink;
}

FBATreeBranchPtr FBAGroupNode::GetRootBranch()
{
	FBATreeBranchPtr TopMost = Branch;
	while (TopMost->ParentBranch)
	{
		TopMost = TopMost->ParentBranch;
	}

	return TopMost;
}

TArray<FBAGroupNodePtr> FBAGroupNode::GetChildNodes(EEdGraphPinDirection Direction, bool bRecursive, bool bInitialDirectionOnly)
{
	TArray<FBAGroupNodePtr> OutChildren;

	const auto& FilterByDirection = [Direction](FBAGroupLink Link)
	{
		return Link.GetDirection() == Direction || Direction == EGPD_MAX;
	};

	TArray<FBAGroupLink> PendingInfo = Children.FilterByPredicate(FilterByDirection);
	while (PendingInfo.Num() > 0)
	{
		FBAGroupLink CurrentInfo = PendingInfo.Pop();
		if (OutChildren.Contains(CurrentInfo.GetToNode()))
		{
			continue;
		}

		OutChildren.Push(CurrentInfo.GetToNode());

		if (Direction != CurrentInfo.GetDirection() && Direction != EGPD_MAX)
		{
			continue;
		}

		if (bRecursive)
		{
			TArray<FBAGroupLink> NextChildren = bInitialDirectionOnly
				? CurrentInfo.GetToNode()->Children
				: CurrentInfo.GetToNode()->Children.FilterByPredicate(FilterByDirection);

			for (FBAGroupLink& Info : NextChildren)// : CurrentInfo->Children.FilterByPredicate(FilterByDirection))
			// for (FBAGroupLink& Info : CurrentInfo.GetFromNode()->Children)
			{
				PendingInfo.Push(Info);
			}
		}
	}

	return OutChildren;
}

FBAGroupNodePtr FBAGroupNode::GetPreviousNode()
{
	int32 CurrIndex = Branch->BranchNodes.IndexOfByKey(AsShared());
	if (CurrIndex > 0)
	{
		return Branch->BranchNodes[CurrIndex - 1];
	}

	return nullptr;
}

TArray<FBANodePtr> FBAGroupNode::GetContainedNodes(bool bIncludeSubNodes)
{
	TArray<FBANodePtr> OutNodes;
	OutNodes.Append(Nodes);

	if (bIncludeSubNodes)
	{
		for (FBAGroupNodePtr SubNode : GetSubNodes(true))
		{
			OutNodes.Append(SubNode->Nodes);
		}
	}

	return OutNodes;
}

void FBAGroupNode::SetX(int32 X)
{
	NodePosX = X;
	UpdateNodeObjects();
}

void FBAGroupNode::SetY(int32 Y)
{
	NodePosY = Y;
	UpdateNodeObjects();
}

FSlateRect FBAGroupNode::GetBounds()
{
	return FSlateRect::FromPointAndExtent(GetPos(), Size);
}

FSlateRect FBAGroupNode::GetSubNodeBounds()
{
	return GetBounds();
}

TArray<FBAGroupPinPtr> FBAGroupNode::GetPins(EEdGraphPinDirection Direction)
{
	TArray<FBAGroupPinPtr> Out;

	// TODO sort pins as they get added
	Pins.StableSort([](const FBAGroupPinPtr& A, const FBAGroupPinPtr& B)
	{
		return A->CachedPinOffset < B->CachedPinOffset;
	});

	for (FBAGroupPinPtr Pin : Pins)
	{
		if (Pin->Direction == Direction || Direction == EGPD_MAX)
		{
			Out.Add(Pin);
		}
	}

	return Out;
}

TArray<FBAGroupLink> FBAGroupNode::GetLinks(EEdGraphPinDirection Direction)
{
	TArray<FBAGroupLink> Out;
	for (FBAGroupPinPtr Pin : GetPins())
	{
		if (Pin->Direction == Direction || Direction == EGPD_MAX)
		{
			Out.Append(Pin->GetLinks());
		}
	}
	return Out;
}

TArray<FBAGroupNodePtr> FBAGroupNode::GetLinkedNodes(EEdGraphPinDirection Direction)
{
	TArray<FBAGroupNodePtr> Out;
	for (auto Link : GetLinks(Direction))
	{
		Out.Add(Link.GetToNode());
	}
	return Out;
}

inline FBAVector2 FBAGroupNode::GetContainedNodePos(FBANodePtr Node)
{
	check(NodeOffsets.Contains(Node));
	return GetPos() + NodeOffsets[Node];
}

void FBAGroupNode::UpdateNodeObjects()
{
	for (FBANodePtr Obj : Nodes)
	{
		check(NodeOffsets.Contains(Obj));
		const FBAVector2& Offset = NodeOffsets[Obj];
		Obj->NodePosX = NodePosX + Offset.X;
		Obj->NodePosY = NodePosY + Offset.Y;
	}
}

TArray<FBATreeBranchPtr> FBAGroupNode::GetSubBranches()
{
	TArray<FBATreeBranchPtr> OutBranches;

	if (Branch)
	{
		for (FBATreeBranchPtr SubBranch : Branch->SubBranches)
		{
			if (SubBranch->Link.GetFromNode().Get() == this)
			{
				OutBranches.Add(SubBranch);
			}
		}
	}

	return OutBranches;
}

int FBAGroupNode::GetNodeIndex()
{
	return Branch->BranchNodes.IndexOfByKey(AsShared());
}

TArray<FBAPinPtr> FBAGroupNode::GetBAPins(EEdGraphPinDirection Direction)
{
	TArray<FBAPinPtr> Out;
	Algo::Transform(GetPins(Direction), Out, &FBAGroupPin::PinHandle);
	return Out;
}

void FBAGroupNode::GetAllNodesInDir(TSet<FBAGroupNodePtr>& OutNodes, EEdGraphPinDirection Dir)
{
	TArray<FBAGroupNodePtr> Pending = { AsShared() };
	while (Pending.Num())
	{
		FBAGroupNodePtr Next = Pending.Pop();

		for (FBAGroupNodePtr Node : Next->GetLinkedNodes(Dir))
		{
			if (OutNodes.Contains(Node))
			{
				continue;
			}

			OutNodes.Add(Node);
			Pending.Add(Node);
		}
	}
}

FBAGroupNode_Comment::FBAGroupNode_Comment(const FBAGroupGraphPtr& InGraph, TSharedPtr<FBACommentContainsNode> InCommentNode)
{
	Graph = InGraph;
	ContainsNode = InCommentNode;

	// map the contains node to our group node
	for (TSharedPtr<FBACommentContainsNode> ChildComment : InCommentNode->Children)
	{
		TSharedRef<FBAGroupNode_Comment> SubComment = MakeShared<FBAGroupNode_Comment>(InGraph, ChildComment);
		SubNodes.Add(SubComment);
		SubComments.Add(SubComment);
	}

	// our subnodes = the contains comments's owned nodes + any child comments
	for (UEdGraphNode* OwnedNode : InCommentNode->OwnedNodes)
	{
		if (FBAGroupNodePtr Node = Graph->FindNode(OwnedNode))
		{
			SubNodes.Add(Node);
		}
	}

	NodeName = InCommentNode->Comment->NodeComment;
}

TArray<FBAGroupNodePtr> FBAGroupNode_Comment::GetSubNodes(bool bRecursive)
{
	if (!bRecursive)
	{
		return SubNodes;
	}

	struct FLocal
	{
		static void GetSubNodes(FBAGroupNodePtr Curr, TArray<FBAGroupNodePtr>& OutNodes)
		{
			TArray<FBAGroupNodePtr> SubNodes = Curr->GetSubNodes(false);
			OutNodes.Append(SubNodes);
			for (auto Node : SubNodes)
			{
				GetSubNodes(Node, OutNodes);
			}
		}
	};


	TArray<FBAGroupNodePtr> OutNodes;
	FLocal::GetSubNodes(AsShared(), OutNodes);

	return OutNodes;
}

TArray<FBAGroupPinPtr> FBAGroupNode_Comment::GetPins(EEdGraphPinDirection Direction)
{
	TArray<FBAGroupPinPtr> OutChildren;
	for (FBAGroupNodePtr SubNode : SubNodes)
	{
		for (FBAGroupLink Link : SubNode->GetLinks(Direction))
		{
			if (SubNodes.Contains(Link.GetToNode()))
			{
				continue;
			}

			OutChildren.Add(Link.From);
		}
	}

	return OutChildren;
}

TArray<FBAGroupLink> FBAGroupNode_Comment::GetLinks(EEdGraphPinDirection Direction)
{
	TSet<FBAGroupLink> OutLinks;
	TArray<FBAGroupNodePtr> AllSubNodes = GetSubNodes(true);

	// filter subnode links which connect outside the comment 
	for (FBAGroupNodePtr SubNode : AllSubNodes)
	{
		TArray<FBAGroupLink> OutsideLinks = SubNode->GetLinks(Direction).FilterByPredicate([&](const FBAGroupLink& Link)
		{
			return !AllSubNodes.Contains(Link.GetToNode());
		});

		OutLinks.Append(OutsideLinks);
	}

	return OutLinks.Array();
}

TArray<FBAGroupNodePtr> FBAGroupNode_Comment::GetChildNodes(EEdGraphPinDirection Direction, bool bRecursive, bool bInitialDirectionOnly)
{
	TSet<FBAGroupNodePtr> OutChildren;
	TArray<FBAGroupNodePtr> AllSubNodes = GetSubNodes(true);
	for (FBAGroupNodePtr SubNode : SubNodes)
	{
		for (FBAGroupNodePtr Child : SubNode->GetChildNodes(Direction, bRecursive, bInitialDirectionOnly))
		{
			if (!AllSubNodes.Contains(Child))
			{
				OutChildren.Add(Child);
			}
		}
	}

	return OutChildren.Array();
}

FSlateRect FBAGroupNode_Comment::GetBounds()
{
	TArray<FSlateRect> SubBounds;
	for (FBAGroupNodePtr SubNode : GetSubNodes(false))
	{
		SubBounds.Add(SubNode->GetBounds());
	}

	const float TitlebarHeight = FBAUtils::GetCachedNodeBounds(Graph->GraphHandler, ContainsNode->Comment, true).GetSize().Y;
	const FIntPoint& Padding = UBASettings::Get().CommentNodePadding;

	return FBAUtils::GetGroupedBounds(SubBounds).ExtendBy(FMargin(
		Padding.X,
		Padding.Y + TitlebarHeight,
		Padding.X,
		Padding.Y));
}

FSlateRect FBAGroupNode_Comment::GetSubNodeBounds()
{
	TOptional<FSlateRect> SubBounds;
	for (FBAGroupNodePtr SubNode : GetSubNodes(false))
	{
		if (!SubBounds)
		{
			SubBounds = SubNode->GetSubNodeBounds();
		}
		else
		{
			SubBounds = SubBounds->Expand(SubNode->GetSubNodeBounds());
		}
	}

	return SubBounds.GetValue();
}

FBATreeBranchPtr FBAGroupNode_Comment::GetBranch()
{
	TArray<FBATreeBranchPtr> AllBranches;
	for (FBAGroupNodePtr Node : GetSubNodes(true))
	{
		AllBranches.Add(Node->GetBranch());
	}

	FBATreeBranchPtr HighestBranch;
	for (FBATreeBranchPtr CurrBranch : AllBranches)
	{
		if (!HighestBranch)
		{
			HighestBranch = CurrBranch;
		}
		else if (HighestBranch->IsSubBranchOf(CurrBranch))
		{
			HighestBranch = CurrBranch;
		}
	}

	return HighestBranch;
}

// ======================================
// Graph
// ======================================

void FBAGroupGraph::MakeGraph(FBAGraphPtr InFullGraph, TSharedPtr<FBAGraphHandler> InGraphHandler, const TArray<UEdGraphNode*>& EdGraphNodes)
{
	TArray<TSet<FBANodePtr>> Groups; 

	for (auto Node : EdGraphNodes)
	{
		auto NodePtr = InFullGraph->FindNode(Node);
		Groups.Add({NodePtr});
	}

	MakeGraph(InFullGraph, InGraphHandler, Groups);
}

void FBAGroupGraph::MakeGraph(FBAGraphPtr InFullGraph, TSharedPtr<FBAGraphHandler> InGraphHandler, const TSet<FBANodePtr>& NodePtrs)
{
	TArray<TSet<FBANodePtr>> GroupedNodes;
	GroupedNodes.Reserve(NodePtrs.Num());
	for (auto Node : NodePtrs)
	{
		GroupedNodes.Add({Node});
	}

	MakeGraph(InFullGraph, InGraphHandler, GroupedNodes);
}

void FBAGroupGraph::MakeGraph(FBAGraphPtr InFullGraph, TSharedPtr<FBAGraphHandler> InGraphHandler, const TArray<TSet<FBANodePtr>>& NodePtrsArr)
{
	GraphHandler = InGraphHandler;
	FullGraph = InFullGraph;

	for (const TSet<FBANodePtr>& NodesPtrs : NodePtrsArr)
	{
		// FSlateRect Bounds = FSlateRect::FromPointAndExtent(FVector2D(NodeObj->NodePosX, NodeObj->NodePosY), GraphHandler->GetNodeSize(NodeObj));
		TArray<FBANodePtr> Objs;
		TArray<FSlateRect> Rects;
		for (auto N : NodesPtrs)
		{
			Objs.Add(N);
			NodeTree.Add(N);
			Rects.Add(N->GetBounds());
		}
		FSlateRect Bounds = FBAUtils::GetGroupedBounds(Rects);
		
		TSharedRef<FBAGroupNode> NewNode = MakeShared<FBAGroupNode>(AsShared(), Objs, Bounds, *GetNameSafe(NodesPtrs[FSetElementId::FromInteger(0)]));
		NewNode->InitPins();
		Nodes.Add(NewNode);
		for (auto Node : NewNode->Nodes)
		{
			NodeMapping.Add(Node, NewNode.ToSharedPtr());
		}
	}

	InitConnections();
}

TSet<TSharedPtr<FBAGroupNode_Comment>> FBAGroupGraph::MakeCommentGraph(TArray<FBAGroupNodePtr>& RootGraph, const TSet<TSharedPtr<FBACommentContainsNode>>& RootContainsComments)
{
	// remove any nodes which were inside a comment from the root graph
	// and replace it with the containing comment

	// UE_LOG(LogTemp, Warning, TEXT("Making root graph"))
	// for (FBAGroupNodePtr N : RootGraph)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("\t%s"), *GetNameSafe(N))
	// }

	TSet<TSharedPtr<FBAGroupNode_Comment>> OutCommentGroupNodes;
	for (auto ContainsNode : RootContainsComments)
	{
		TSharedRef<FBAGroupNode_Comment> GroupComment = MakeShared<FBAGroupNode_Comment>(AsShared(), ContainsNode);
		// UE_LOG(LogTemp, Warning, TEXT("Made comment %s"), *GetNameSafe(GroupComment));
		for (FBAGroupNodePtr SubNode : GroupComment->GetSubNodes(true))
		{
			// UE_LOG(LogTemp, Warning, TEXT("\t\tRemoving sub node %s"), *GetNameSafe(SubNode));
			RootGraph.Remove(SubNode);
		}

		RootGraph.Add(GroupComment);
		OutCommentGroupNodes.Add(GroupComment.ToSharedPtr());
	}

	return OutCommentGroupNodes;
}

TArray<FBAGroupNodePtr> FBAGroupGraph::MakeCommentSubset(const TArray<FBAGroupNodePtr>& AllNodes, const TSet<TSharedPtr<FBACommentContainsNode>>& ContainsNodes)
{
	TArray<FBAGroupNodePtr> OutNodes = AllNodes;
	for (auto ContainsNode : ContainsNodes)
	{
		TSharedRef<FBAGroupNode_Comment> GroupComment = MakeShared<FBAGroupNode_Comment>(AsShared(), ContainsNode);
		for (FBAGroupNodePtr SubNode : GroupComment->GetSubNodes(true))
		{
			OutNodes.Remove(SubNode);
		}

		// // make a new group node for the comments and remove it from our node set
		// for (UEdGraphNode* NodeUnderComment : ContainsNode->AllContainedNodes)
		// {
		// 	if (FBAGroupNodePtr NodeUnderGroup = FindNode(NodeUnderComment))
		// 	{
		// 		OutNodes.Remove(NodeUnderGroup);
		// 	}
		// }
		
		OutNodes.Add(GroupComment.ToSharedPtr());
	}

	return OutNodes;
}


void FBAGroupGraph::PrintGraph()
{
	UE_LOG(LogBAGroupGraph, VeryVerbose, TEXT("BEGIN PRINT GROUP GRAPH"));
	for (auto& Node : Nodes)
	{
		UE_LOG(LogBAGroupGraph, VeryVerbose, TEXT("\tNODES %s"), *GetNameSafe(Node));
		for (auto Obj : Node->Nodes)
		{
			UE_LOG(LogBAGroupGraph, VeryVerbose, TEXT("\t\t%s"), *GetNameSafe(Obj));
		}

		for (auto& Pin : Node->Pins)
		{
			if (Pin->LinkedTo.Num())
			{
				UE_LOG(LogBAGroupGraph, VeryVerbose, TEXT("\t\t\tPIN %s %f"), 
					*FBAUtils::GetPinName(Pin->PinHandle->PinHandle.GetPin()),
					Pin->GetPos().Y
				);
			}
		}
	}
	UE_LOG(LogBAGroupGraph, VeryVerbose, TEXT("END PRINT GROUP GRAPH"));
}

void FBAGroupGraph::ApplyGraph(TOptional<FText> Transaction)
{
	for (const FBAGroupNodePtr& Node : Nodes)
	{
		for (auto NodeObj : Node->Nodes)
		{
			FBAVector2 Pos = Node->GetContainedNodePos(NodeObj);
			NodeObj->NodePosX = Pos.X;
			NodeObj->NodePosY = Pos.Y;
			// Graph->GetSchema()->SetNodePosition(NodeObj, Node->GetNodeObjectPos(NodeObj));
		}
	}

	FullGraph->ApplyGraph(Transaction);
}

void FBAGroupGraph::MakeSnapshot()
{
	for (const FBAGroupNodePtr& Node : Nodes)
	{
		for (FBANodePtr NodeObj : Node->Nodes)
		{
			FBAVector2 Pos = Node->GetContainedNodePos(NodeObj);
			NodeObj->NodePosX = Pos.X;
			NodeObj->NodePosY = Pos.Y;
		}
	}
}

void FBAGroupGraph::InitConnections()
{
	TMap<FBAPinPtr, FBAGroupPinPtr> PinMap;

	for (auto& Node : Nodes)
	{
		for (auto& Pin : Node->Pins)
		{
			// UE_LOG(LogTemp, Warning, TEXT("PIN %p %s"), Pin->PinHandle.GetPin(), *Pin->PinHandle.PinId.ToString());
			PinMap.Add(Pin->PinHandle, Pin);
		}
	}

	for (auto& Node : Nodes)
	{
		for (auto& TreePin : Node->Pins)
		{
			FBAPinPtr GraphPin = TreePin->PinHandle;
			check(GraphPin);

			for (FBAPinPtr LinkedGraphPin : GraphPin->LinkedTo)
			{
				if (!NodeTree.Contains(LinkedGraphPin->GetOwningNode()))
				{
					continue;
				}

				if (FBAGroupPinPtr LinkedPin = PinMap.FindRef(LinkedGraphPin))
				{
					TreePin->LinkedTo.Add(LinkedPin);
				}
			}
		}
	}
}
