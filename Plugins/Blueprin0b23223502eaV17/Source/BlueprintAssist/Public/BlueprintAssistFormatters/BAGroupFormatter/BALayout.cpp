// Copyright fpwong. All Rights Reserved.

#include "BALayout.h"

#include "BAGraphSnapshotManager.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistStats.h"
#include "BlueprintAssistUtils.h"
#include "SGraphNodeComment.h"
#include "SGraphPanel.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor/BlueprintGraph/Classes/K2Node_Knot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Stats/StatsMisc.h"

FBALayout::FBALayout(
	TSharedPtr<FFormatterInterface> InFormatter,
	TSharedPtr<FBAGraphHandler> InGraphHandler,
	FBAGraphPtr InMasterGraph,
	FBANodePtr InMasterRootNode,
	EEdGraphPinDirection InGraphDirection,
	const FBALayoutConfig& Config,
	TFunction<bool(const FBAPinLink&)> InLinkFilter,
	TFunction<bool(FBAPin&)> InPinFilter) 
	: Tree()
{
	NodePadding = Config.NodePadding;
	VerticalPinSpacing = Config.VerticalPinSpacing;
	bCenterBranches = Config.bCenterBranches;
	LayoutType = Config.LayoutType;

	if (LayoutType == EBALayoutType::Params)
	{
		bApplyHelixing = UBASettings::Get().ParameterStyle == EBAParameterFormattingStyle::Helixing;
	}

	Formatter = InFormatter;
	GraphHandler = InGraphHandler;
	MasterGraph = InMasterGraph;
	MasterRootNode = InMasterRootNode;
	GraphDirection = InGraphDirection;
	LinkFilter = InLinkFilter;
	PinFilter = InPinFilter;

	Tree.bForceBranchesFromRoot = Config.bForceBranchFromRoot;
}

void FBALayout::InitGroupGraph()
{
	InitNodePool(MasterRootNode, NodePool);

	GroupGraph = MakeShared<FBAGroupGraph>();
	GroupGraph->LinkFilter = PinFilter; 
	GroupGraph->MakeGraph(MasterGraph, GraphHandler, NodePool);
	GroupRootNode = GroupGraph->FindNode(MasterRootNode);
	Tree.BuildTree(GraphHandler, GroupRootNode, GraphDirection);

	GroupGraph->PrintGraph();
}

void FBALayout::InitGroupGraph(const TArray<TSet<FBANodePtr>>& NodeGroups)
{
	InitNodePool(MasterRootNode, NodePool);
	GroupGraph = MakeShared<FBAGroupGraph>();
	GroupGraph->LinkFilter = PinFilter; 
	GroupGraph->MakeGraph(MasterGraph, GraphHandler, NodeGroups);
	GroupRootNode = GroupGraph->FindNode(MasterRootNode);
	Tree.BuildTree(GraphHandler, GroupRootNode, GraphDirection);

	GroupGraph->PrintGraph();
}

void FBALayout::FormatNode(UEdGraphNode* InitialNode)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBAGraphFormatterTree::FormatNode"), STAT_EdGraphFormatter_FormatNode, STATGROUP_BA_EdGraphFormatter);
	UE_LOG(LogBlueprintAssist, Verbose, TEXT("Formatting with BAFormatterTree"))

	InitialNode->GetGraph()->Modify();
}

void FBALayout::FormatX()
{
	FBAScopedSnapshotGroup G1("LayoutX1", GroupGraph);
	LayoutFormatXTree();
}

void FBALayout::FormatY()
{
	{
		if (bApplyHelixing)
		{
			FBAScopedSnapshotGroup G("ApplyHelixing", GroupGraph);
			ApplyHelixing();
		}

		{
			FBAScopedSnapshotGroup G("AvoidPreviousPin", GroupGraph);
			AvoidPreviouslyLinkedPin();
		}

		{
			FBAScopedSnapshotGroup G("LayoutFormatY", GroupGraph);
			LayoutFormatYTree();
		}
	}
}

void FBALayout::AvoidPreviouslyLinkedPin()
{
	// move branches to avoid the previously linked pin
	for (FBATreeBranchPtr Branch : Tree.Branches)
	{
		UE_LOG(LogFormatY, Log, TEXT("[%hs] Checking avoid pin for branch %s"), __FUNCTION__, *Branch->ToString());

		if (FBAGroupPinPtr FromPin = Branch->Link.From)
		{
			FBAPinPtr Prev = FromPin->PinHandle->GetPreviousPin();
			while (Prev)
			{
				if (Prev->LinkedTo.Num())
				{
					const auto& BranchRootNodes = Branch->Link.GetToNode()->Nodes;
					const bool bAvoidPin = Prev->LinkedTo.ContainsByPredicate([&](FBAPinPtr Pin)
					{
						return !BranchRootNodes.Contains(Pin->GetOwningNode());
					});

					UE_LOG(LogFormatY, Log, TEXT("[%hs] Checking avoid %s for pin %s"), __FUNCTION__, *Branch->ToString(), *Prev->GetName(true));

					if (bAvoidPin)
					{
						const float Pos = Prev->GetPos().Y + UBASettings::Get().ParameterVerticalPinSpacing;
						const float Delta = Pos - Branch->GetBranchBounds().Top;

						if (Delta > 0)
						{
							UE_LOG(LogFormatY, Warning, TEXT("[%hs] Moved Branch %s avoid pin %s (%f)"), __FUNCTION__, *Branch->ToString(), *FromPin->GetName(), Delta);
							Branch->MoveBranchY(Delta);
							FBAScopedSnapshotGroup::MakeLeafSnapshot(FString::Printf(TEXT("Avoid linked pin %s"), *Prev->GetName()), GroupGraph);
						}

						break;
					}
				}

				Prev = Prev->GetPreviousPin();
			}
		}
	}
}

void FBALayout::ApplyHelixing()
{
	TSet<FBAGroupLink> VisitedLinks;
	TSet<FBAGroupNodePtr> Straightened;

	TArray<FBATreeBranchPtr> AllSubBranches;
	Tree.RootBranch->GetAllSubBranches(AllSubBranches, GraphDirection);
	if (AllSubBranches.Num() != 1)
	{
		return;
	}

	// check if any single node is extremely tall
	if (UBASettings::Get().bLimitHelixingHeight)
	{
		float TotalHeight = 0;
		for (FBATreeBranchPtr Branch : Tree.RootBranch->GetSubBranches(GraphDirection))
		{
			for (FBAGroupNodePtr Node : Branch->BranchNodes)
			{
				float NodeHeight = Node->GetBounds().GetSize().Y;
				if (NodeHeight > UBASettings::Get().HelixingHeightMax)
				{
					return;
				}

				TotalHeight += NodeHeight;

				if (TotalHeight > UBASettings::Get().HelixingHeightMax)
				{
					return;
				}
			}
		}

		// helixing should not apply if the height of the stack is too large
		if (TotalHeight > UBASettings::Get().HelixingHeightMax)
		{
			return;
		}
	}

	FSlateRect PrevBounds = Tree.RootBranch->GetBranchBounds();
	const float AlignX = PrevBounds.Left;

	for (FBATreeBranchPtr Branch : Tree.RootBranch->GetSubBranches(GraphDirection))
	{
		for (FBAGroupNodePtr Node : Branch->BranchNodes)
		{
			Node->SetX(AlignX);
			Node->SetY(PrevBounds.Bottom + NodePadding.Y);
			PrevBounds = Node->GetBounds();
			FBAScopedSnapshotGroup::MakeLeafSnapshot("ApplyHelixing", GroupGraph);
		}

		for (FBATreeBranchPtr SubBranch : Branch->SubBranches)
		{
			if (SubBranch->Link.HasBothLinks())
			{
				SubBranch->StraightenBranch(EGPD_Output);
				float DeltaX = SubBranch->Link.GetDirection() == EGPD_Output
					? SubBranch->Link.GetFromNode()->GetBounds().Right + NodePadding.X - SubBranch->GetBranchBounds().Left
					: SubBranch->Link.GetNode()->GetBounds().Left - NodePadding.X - SubBranch->GetBranchBounds().Right;

				SubBranch->MoveBranchX(DeltaX);
			}
		}
	}
}

void FBALayout::InitComments()
{
	if (!UBASettings::Get().bApplyCommentPadding)
	{
		return;
	}

	if (FCommentHandler* CommentHandler = GetCommentHandler())
	{
		CommentHandler->BuildTree();
		RootComments = GroupGraph->MakeCommentGraph(GroupGraph->Nodes, CommentHandler->GetRootNodes());
	}
}

void FBALayout::MoveParamsBelowExecLine()
{
	if (LayoutType != EBALayoutType::Params)
	{
		return;
	}

	if (UBASettings_Advanced::HasDebugSetting("Baseline"))
	{
		return;
	}

	// only move below baseline if our root is an exec node
	if (!MasterRootNode->IsExecNode())
	{
		return;
	}

	for (auto Dir : {EGPD_Input, EGPD_Output})
	{
		TArray<TSharedPtr<FBAPin>> ExecPins = MasterRootNode->GetPins(Dir).FilterByPredicate(&FBAPin::IsExecPin);
		if (ExecPins.IsEmpty())
		{
			continue;
		}

		FBAPinPtr FirstPin = ExecPins[0];

		int32 Baseline = FMath::RoundToInt(FirstPin->GetPos().Y + UBASettings::Get().ParameterVerticalPinSpacing);

		FBAScopedSnapshotGroup G("MoveParamsBelowExecLine");
		for (FBATreeBranchPtr Branch : Tree.Branches)
		{
			if (Branch->Link.HasBothLinks() && Branch->Link.GetFromNode() == GroupRootNode)
			{
				if (Branch->Link.GetDirection() != Dir)
				{
					continue;
				}

				FSlateRect Bounds = Branch->GetBranchBounds();

				const int Top = FMath::RoundToInt(Bounds.Top);
				if (Baseline > Top)
				{
					const int Delta = Baseline - Top;

					for (FBAGroupNodePtr BranchNode : Branch->BranchNodes)
					{
						BranchNode->MoveY(Delta);
						UE_LOG(LogFormatY, VeryVerbose, TEXT("[%hs] Moved node %s by %d"), __FUNCTION__, *GetNameSafe(BranchNode), Delta);
					}

					FBAScopedSnapshotGroup::MakeLeafSnapshot(Branch->ToString(), GroupGraph);
				}
			}
		}
	}
}

void FBALayout::ExpandNodesAheadOfParameters()
{
	// DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FEdGraphFormatter::ExpandNodesAheadOfParameters"), STAT_EdGraphFormatter_ExpandNodesAheadOfParameters, STATGROUP_BA_EdGraphFormatter);
	if (!UBASettings::Get().bExpandNodesAheadOfParameters)
	{
		return;
	}

	FBAScopedSnapshotGroup G("ExpandAheadOfParameters", GroupGraph);

	const FIntPoint& ParamPadding = UBASettings::Get().BlueprintParameterPadding;

	UE_LOG(LogFormatX, Log, TEXT("Expand nodes ahead of params"))

	// iter each node, checking input params and push the node to the right to match the correct spacing to our param node
	for (auto Node : NodePool)
	{
		if (!Node->IsExecNode())
		{
			continue;
		}

		FBAGroupNodePtr GroupNode = GroupGraph->FindNode(Node);
		if (!GroupNode)
		{
			UE_LOG(LogFormatX, Error, TEXT("%hs - Failed to find group node %s"), __FUNCTION__, *GetNameSafe(Node));
			continue;
		}

		TArray<FBAPinLink> Params = Node->GetLinks(EGPD_Input).FilterByPredicate(&FBAPinLink::IsParameter);
		for (FBAPinLink& ParamLink : Params)
		{
			FBANodePtr ParamNode = ParamLink.GetToNode();
			if (!ParamNode->IsParameterNode())
			{
				continue;
			}

			// we only want to expand ahead for nodes outside our group
			if (GroupNode->Nodes.Contains(ParamNode))
			{
				continue;
			}

			const int32 Delta = ParamNode->GetBounds().Right + ParamPadding.X - Node->NodePosX;

			if (Delta > 0)
			{
				UE_LOG(LogFormatX, Log, TEXT("\tExpand node ahead of %s %s %d"), *GetNameSafe(Node), *GetNameSafe(ParamNode), Delta);
				{
					GroupNode->MoveX(Delta);
					for (auto Child : GroupNode->GetChildNodes())
					{
						Child->MoveX(Delta);
					}

					FBAScopedSnapshotGroup::MakeLeafSnapshot(FString::Printf(TEXT("Expand %s ahead of %s"), *GetNameSafe(Node), *GetNameSafe(ParamNode)), GroupGraph);
				}
			}
		}
	}
}

void FBALayout::InitNodePool(FBANodePtr InitialNode, TSet<FBANodePtr>& OutNodePool)
{
	TArray<FBANodePtr> NodeStack;

	NodeStack.Push(InitialNode);

	while (!NodeStack.IsEmpty())
	{
		FBANodePtr CurrentNode = NodeStack.Pop();

		OutNodePool.Add(CurrentNode);

		for (const FBAPinLink& Link : CurrentNode->GetLinks())
		{
			FBANodePtr LinkedNode = Link.GetToNode();
			if (OutNodePool.Contains(LinkedNode))
			{
				continue;
			}

			if (!IsValidLink(Link))
			{
				continue;
			}

			NodeStack.Push(LinkedNode);
		}
	}
}

void FBALayout::LayoutFormatXTree()
{
	DecideXParents({FBAGroupLink(Tree.RootBranch->RootNode, GraphDirection)});

	if (UBASettings::Get().FormattingStyle == EBANodeFormattingStyle::Expanded)
	{
		for (auto Branch : Tree.Branches)
		{
			if (Branch->bIsRootBranch)
			{
				if (Branch->NodeToAvoid && Branch->ParentBranch)
				{
					int NewRootPos = Branch->NodeToAvoid->GetBounds().Right + NodePadding.X;

					// TODO use this as a style
					if (BA_DEBUG("AlternateExpandStyle"))
					{
						NewRootPos = Branch->NodeToAvoid->GetX();
					}

					Branch->RootNode->SetX(FMath::Max(Branch->RootNode->GetX(), NewRootPos));
					FBAScopedSnapshotGroup::MakeLeafSnapshot("FixRootPositionX", GroupGraph);
				}

				DecideXParents({FBAGroupLink(Branch->RootNode, GraphDirection)});
			}
		}
	}
}

int32 FBALayout::GetChildX(const FBAGroupLink& Link)
{
	if (Link.From == nullptr)
	{
		return Link.GetNode()->GetBounds().Left;
	}

	auto Parent = Link.GetFromNode();
	auto Child = Link.GetToNode();

	if (Parent == nullptr)
	{
		return 0;
	}

	FSlateRect ParentBounds = Parent->GetBounds();

	if (Child == nullptr)
	{
		return ParentBounds.Left;
	}

	float NewNodePos;

	FSlateRect ChildBounds = Child->GetBounds();

	if (Link.GetDirection() == EGPD_Input)
	{
		NewNodePos = ParentBounds.Left - NodePadding.X - ChildBounds.GetSize().X;
	}
	else
	{
		NewNodePos = ParentBounds.Right + NodePadding.X;
	}

	NewNodePos = FBAUtils::AlignTo8x8Grid(NewNodePos);

	// UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("Node %s New Node Pos %d | %s | %s | %s"), *Link.ToString(), FMath::CeilToInt(NewNodePos), *ParentBounds.ToString(), *ChildBounds.ToString(), *LargerBounds.ToString());

	return FMath::CeilToInt(NewNodePos);
}

void FBALayout::DecideXParents(TArray<FBAGroupLink> InitialLinks)
{
	TArray<FBAGroupLink> PrimaryStack;
	TArray<FBAGroupLink> SecondaryStack;

	FBADrawBuilder Draw;

	for (const FBAGroupLink& Link : InitialLinks)
	{
		if (Link.GetDirection() == GraphDirection)
		{
			PrimaryStack.Push(Link);
		}
		else
		{
			SecondaryStack.Push(Link);
		}
	}

	EEdGraphPinDirection CurrentDirection = GraphDirection;

	int Count = 0;
	while (!PrimaryStack.IsEmpty() || !SecondaryStack.IsEmpty())
	{
		// reset every direction switch, don't iterate these twice
		TSet<FBAGroupLink> ParsedLinks;
		TArray<FBAGroupLink>& CurrentStack = CurrentDirection == GraphDirection ? PrimaryStack : SecondaryStack;

		while (!CurrentStack.IsEmpty())
		{
			if (++Count >= 500)
			{
				UE_LOG(LogFormatX, Log, TEXT("[%hs] Stuck in infinite loop! Exiting early."), __FUNCTION__)
				return;
			}

			FBAGroupLink FromLink = CurrentStack.Pop();
			if (Tree.CycleLinks.Contains(FromLink))
			{
				continue;
			}

			FBAGroupNodePtr CurrentNode = FromLink.GetNode();
			FBATreeBranchPtr CurrentBranch = CurrentNode->GetBranch();

			// UE_LOG(LogFormatX, Log, TEXT("Iter node %s"), *GetNameSafe(CurrentNode));

			UE_LOG(LogFormatX, Log, TEXT("Processing %s"), *FromLink.ToString());
			const int32 NewX = GetChildX(FromLink);

			bool bShouldIterate = false;

			// we don't have a parent
			if (!CurrentNode->GetParentNode() || !CurrentNode->ParentLink.HasBothLinks())
			{
				// if (!(CurrentBranch->bIsRootBranch && CurrentBranch->RootNode == CurrentNode))
				if (CurrentNode != Tree.RootNode)
				{
					UE_LOG(LogFormatX, Log, TEXT("\tTaking no parent"));
					CurrentNode->SetParentLink(FromLink);
					CurrentNode->SetX(NewX);
				}
				else
				{
					UE_LOG(LogFormatX, Log, TEXT("\tSkip parent for root branch node"));
				}

				bShouldIterate = true;
			}
			else
			{
				const FBAGroupLink& CurrParent = CurrentNode->ParentLink;
				const int32 OldX = CurrentNode->GetX();

				// pull the node towards us
				const bool bOnlyUpdateLocation = (FromLink <=> CurrParent) && (NewX != OldX) && (!FromLink.IsOnSameBranch());

				bool bPositionIsBetter
					= CurrentDirection == EGPD_Output
					? NewX > OldX
					: NewX < OldX;

				UE_LOG(LogFormatX, Log, TEXT("\tComparing parents Old: %s (%d) New: %s (%d) (%d)"),
						*CurrParent.ToString(), OldX,
						*FromLink.ToString(), NewX,
						static_cast<int>(CurrParent.From->Direction));

				if (bPositionIsBetter || bOnlyUpdateLocation)
				{
					CurrentNode->SetX(NewX);

					if (!bOnlyUpdateLocation)
					{
						CurrentNode->SetParentLink(FromLink);
					}

					bShouldIterate = true;
				}
			}

			if (!FromLink.HasBothLinks())
			{
				bShouldIterate = true;
			}

			FBAScopedSnapshotGroup::MakeLeafSnapshot(FString::Printf(TEXT("DecideX: %s"), *FromLink.ToString()), GroupGraph);


			// don't iterate this node if this node hasn't changed (the parent changed)
			if (!bShouldIterate)
			{
				UE_LOG(LogFormatX, Log, TEXT("\tHas not changed, skipping!"));
				continue;
			}


			// debug draw
			{
				if (FromLink.HasBothLinks())
				{
					Draw.Add(MakeShared<FBADrawPinLinkAction>(FromLink.From->PinHandle->GetObj(), FromLink.To->PinHandle->GetObj()));
				}
			}

			ParsedLinks.Add(FromLink);

			UE_LOG(LogFormatX, Log, TEXT("Iter links %s %d"), *GetNameSafe(CurrentNode), CurrentNode->GetLinks().Num());
			for (FBAGroupLink Link : CurrentNode->GetLinks())
			{
				UE_LOG(LogFormatX, Log, TEXT("Iter link %s"), *Link.ToString());
				// if (!Link.IsExecLink())
				// {
				// 	continue;
				// }

				if (Tree.CycleLinks.Contains(Link))
				{
					continue;
				}

				if (CurrentDirection == GraphDirection && ParsedLinks.Contains(Link.Opposite()))
				{
					UE_LOG(LogFormatX, Log, TEXT("Skipping parsed link %s"), *Link.ToString());
					continue;
				}

				auto ToBranch = Link.GetToNode()->GetBranch();
				if (!ToBranch->NodeToAvoid)
				{
					if (Link.GetDirection() == GraphDirection)
					{
						ToBranch->NodeToAvoid = Link.GetFromNode();
						UE_LOG(LogFormatX, Log, TEXT("Set avoid node for branch %s to %s"), *CurrentBranch->ToString(), *GetNameSafe(ToBranch->NodeToAvoid));
					}
					else
					{
						ToBranch->NodeToAvoid = Link.GetFromNode()->GetPreviousNode();
						UE_LOG(LogFormatX, Log, TEXT("Set avoid node (previous) for branch %s to %s"), *CurrentBranch->ToString(), *GetNameSafe(ToBranch->NodeToAvoid));
					}
				}

				if (Link.GetDirection() == GraphDirection)
				{
					UE_LOG(LogFormatX, Log, TEXT("PUSH out stack %s"), *Link.ToString());
					PrimaryStack.Push(MoveTemp(Link));
				}
				else
				{
					// if (!Link.IsOnSameBranch())
					{
						UE_LOG(LogFormatX, Log, TEXT("PUSH in stack %s"), *Link.ToString());
						SecondaryStack.Push(MoveTemp(Link));
					}
				}
			}
		}

		CurrentDirection = UEdGraphPin::GetComplementaryDirection(CurrentDirection);
	}

	if (BA_DEBUG("DecideX"))
	{
		if (auto Overlay = GraphHandler->GetGraphOverlay())
		{
			static uint32 Hash = GetTypeHash(FName("DecideXParents"));
			if (!Overlay->HasDrawAction(Hash))
			{
				Overlay->AddDrawAction(Draw.Queue(1.0f), Hash);
			}
		}
	}
}

void FBALayout::LayoutFormatYTree()
{
	TSet<FBATreeBranchPtr> BranchesToCollideWith;

	MoveParamsBelowExecLine();

	for (auto CurrentBranch : Tree.Branches)
	{
		UE_LOG(LogFormatY, Verbose, TEXT("Layout Branch %s %d"), *CurrentBranch->ToString(), BranchesToCollideWith.Num());
		for (FBATreeBranchPtr ToCollide : BranchesToCollideWith)
		{
			UE_LOG(LogFormatY, Verbose, TEXT("\t%s"), *ToCollide->ToString());
		}

		const float LeftPadding = GraphDirection == EGPD_Output ? NodePadding.X * 0.5f : 0; 
		const float RightPadding = GraphDirection == EGPD_Input ? NodePadding.X * 0.5f : 0; 
		const FMargin CollisionPadding(LeftPadding, 0, RightPadding, NodePadding.Y);
		for (int CollisionLimit = 0; CollisionLimit < 50; CollisionLimit++)
		{
			bool bNoCollision = true;
			for (FBATreeBranchPtr PreviousBranch : BranchesToCollideWith)
			{
				TArray<FBATreeBranchPtr> CurrentBranchesAll;
				CurrentBranch->GetBranches(CurrentBranchesAll);
				if (CurrentBranchesAll.Contains(PreviousBranch))
				{
					UE_LOG(LogFormatY, Verbose, TEXT("\tSkip2 %s"), *PreviousBranch->ToString());
					continue;
				}

				UE_LOG(LogFormatY, Verbose, TEXT("\tChecking against %s"), *PreviousBranch->ToString());

				TArray<FSlateRect> PreviousBoundsArray;
				for (auto Bounds : PreviousBranch->GetBoundsForNodeArray())
				{
					PreviousBoundsArray.Add(Bounds.ExtendBy(CollisionPadding));
				}

				if (!UBASettings_Advanced::HasDebugSetting("DisableWireBounds"))
				{
					for (auto Bounds : PreviousBranch->GetWireBounds(GraphHandler, EGPD_MAX, {CurrentBranch}))
					{
						PreviousBoundsArray.Add(Bounds.ExtendBy(FMargin(0, 0, 0, VerticalPinSpacing)));
					}
				}

				// collide with nodes from our branch and sub branches
				TArray<FSlateRect> CurrentBoundsArray = CurrentBranch->GetBoundsForNodeArray();
				for (const FSlateRect& CurrentBounds : CurrentBoundsArray)
				{
					bool bFoundCollision = false;
					for (const FSlateRect& PreviousBounds : PreviousBoundsArray)
					{
						if (FSlateRect::DoRectanglesIntersect(CurrentBounds, PreviousBounds))
						{
							if (UBASettings_Advanced::HasDebugSetting("DrawYCollision"))
							{
								GraphHandler->GetGraphOverlay()->DrawBounds(CurrentBounds, FColor::Yellow);
								GraphHandler->GetGraphOverlay()->DrawBounds(PreviousBounds, FColor::Red);
							}

							const int32 Delta = (PreviousBounds.Bottom - CurrentBounds.Top) + 1;

							UE_LOG(LogFormatY, Verbose, TEXT("COLLISION %s AND %s %d"), *CurrentBranch->ToString(),
									*PreviousBranch->ToString(), Delta);

							CurrentBranch->SetAvoidBranchY(PreviousBranch);

							CurrentBranch->MoveBranchY(Delta);
							FBAScopedSnapshotGroup::MakeLeafSnapshot(FString::Printf(TEXT("Collision %s"), *CurrentBranch->ToString()), GroupGraph);

							bNoCollision = false;
							bFoundCollision = true;
							break;
						}
					}

					if (bFoundCollision)
					{
						break;
					}
				}
			}

			if (bNoCollision)
			{
				break;
			}
		}

		BranchesToCollideWith.Add(CurrentBranch);
	}
}

void FBALayout::ApplyCommentX()
{
	if (!UBASettings::Get().bApplyCommentPadding)
	{
		return;
	}

	FBAScopedSnapshotGroup Snapshot("ApplyCommentX", GroupGraph);
	ApplyCommentX_Recursive(GroupGraph->Nodes, RootComments);
}

#define DEBUG_COMMENT_X 0
void FBALayout::ApplyCommentX_Recursive(TArray<FBAGroupNodePtr> NodeSet, const TSet<FBAGroupNode_CommentPtr>& Comments)
{
	if (Comments.Num() == 0)
	{
		return;
	}

	const auto LeftMost = [&](const FBAGroupNodePtr& NodeA, const FBAGroupNodePtr& NodeB)
	{
		if (GraphDirection == EGPD_Output)
		{
			// this should use subnodes or something
			return NodeA->GetSubNodeBounds().Left < NodeB->GetSubNodeBounds().Left;
		}
		else
		{
			return NodeA->GetSubNodeBounds().Left > NodeB->GetSubNodeBounds().Left;
		}
	};

	NodeSet.StableSort(LeftMost);

	for (auto GroupComment : Comments)
	{
		ApplyCommentX_Recursive(GroupComment->SubNodes, GroupComment->SubComments);
	}

#if DEBUG_COMMENT_X
	UE_LOG(LogFormatX, Log, TEXT("Processing node set"))
	for (FBAGroupNodePtr Node : NodeSet)
	{
		UE_LOG(LogFormatX, Log, TEXT("\t%s %f"), *GetNameSafe(Node), Node->GetSubNodeBounds().Left);
	}
#endif

	// perform collision checks between nodes and push them to the right
	// since the list is sorted horizontally we can iterate like this
	for (int i = 0; i < NodeSet.Num() - 1; ++i)
	{
		FBAGroupNodePtr A = NodeSet[i];

		TArray<FBAGroupNodePtr> NodesToCollide;
		for (const FBAGroupLink& Link : A->GetLinks(GraphDirection))
		{
			if (Tree.CycleLinks.Contains(Link))
			{
				continue;
			}

			// collide with nodes on the same branch 
			if (Link.IsOnSameBranch())
			{
				NodesToCollide.Add(Link.GetToNode());
			}
			else // otherwise only collide with nodes to the right of the comment (or node)
			{
				if (GraphDirection == EGPD_Output)
				{
					if (Link.GetToNode()->GetBounds().Left > A->GetBounds().Right)
					{
						NodesToCollide.Add(Link.GetToNode());
					}
				}
				else
				{
					if (Link.GetToNode()->GetBounds().Right < A->GetBounds().Left)
					{
						NodesToCollide.Add(Link.GetToNode());
					}
				}
			}
		}

		UE_LOG(LogFormatX, Log, TEXT("Iter %s %d"), *GetNameSafe(A), NodesToCollide.Num());

		// only collide with our child node
		// TArray<FBAGroupNodePtr> Children = A->GetChildNodes();
		for (FBAGroupNodePtr B : NodesToCollide)
		{
			UE_LOG(LogFormatX, Log, TEXT("Checking %s %s"), *GetNameSafe(A), *GetNameSafe(B))

			// when we get our children, some of these won't be inside the current scope of nodes
			// if it is not, then it maybe inside one of the comments in scope (cos comments won't be returned by GetChildNodes)
			if (!NodeSet.Contains(B))
			{
				B = FindParentComment(B, Comments);
			}

			if (!B)
			{
				UE_LOG(LogFormatX, Log, TEXT("\tSkip B invalid"))
				continue;
			}

			// UE_LOG(LogFormatX, Log, TEXT("Iter %s %s"), *GetNameSafe(A), *GetNameSafe(B))
			FSlateRect BoundsA = A->GetBounds().ExtendBy(FMargin(NodePadding.X, 0.f));
			FSlateRect BoundsB = B->GetBounds();

#if DEBUG_COMMENT_X
			GraphHandler->GetGraphOverlay()->DrawBounds(BoundsA, FLinearColor::Blue);
			GraphHandler->GetGraphOverlay()->DrawBounds(BoundsB, FLinearColor::Green);
			UE_LOG(LogFormatX, Log, TEXT("Iter %s %s"), *GetNameSafe(A), *GetNameSafe(B))
#endif

			if (FSlateRect::DoRectanglesIntersect(BoundsA, BoundsB))
			{
#if DEBUG_COMMENT_X
				UE_LOG(LogFormatX, Log, TEXT("\tCollision"))
#endif

				// TODO perhaps this was useful edge case?
				// const float Delta = B->ParentLink.GetDirection() == EGPD_Output
				// 	? BoundsA.Right + 1.0f - BoundsB.Left
				// 	: BoundsA.Left - BoundsB.Right;

				const float Delta = GraphDirection == EGPD_Output
					? BoundsA.Right + 1.0f - BoundsB.Left
					: BoundsA.Left - 1.0f - BoundsB.Right;

				// recursively move children
				TSet<FBAGroupNodePtr> NodesToMove = {B};
				UE_LOG(LogFormatX, Log, TEXT("Moving %s %f"), *GetNameSafe(B), Delta);
				for (FBAGroupNodePtr Child : B->GetChildNodes(EGPD_MAX))
				{
					// UE_LOG(LogFormatX, Log, TEXT("\tChild %s %f"), *GetNameSafe(Child), Delta);
					NodesToMove.Add(Child);
				}

				for (FBAGroupNodePtr Child : B->GetSubNodes(true))
				{
					// UE_LOG(LogFormatX, Log, TEXT("\tSubNode %s %f"), *GetNameSafe(Child), Delta);
					NodesToMove.Add(Child);
				}

				for (auto Node : NodesToMove)
				{
					Node->MoveX(Delta);
					// UE_LOG(LogFormatX, Log, TEXT("\tMoving %s %f"), *GetNameSafe(B), Delta);
				}
			}
		}
	}

	FBAScopedSnapshotGroup::MakeLeafSnapshot("Move", GroupGraph);
}

void FBALayout::ApplyCommentY()
{
	if (!UBASettings::Get().bApplyCommentPadding)
	{
		return;
	}

	FBAScopedSnapshotGroup Snapshot("ApplyCommentY", GroupGraph);
	ApplyCommentY_Recursive(GroupGraph->Nodes, RootComments);
}

void FBALayout::StraightenTree()
{
	FBAScopedSnapshotGroup G("StraightenTree", GroupGraph);
	Tree.StraightenTree();
}

#define DEBUG_COMMENT_Y 0
void FBALayout::ApplyCommentY_Recursive(TArray<FBAGroupNodePtr> NodeSet, const TSet<FBAGroupNode_CommentPtr>& Comments)
{
	if (Comments.Num() == 0)
	{
		return;
	}

	const auto TopMost = [&](const FBAGroupNodePtr& NodeA, const FBAGroupNodePtr& NodeB)
	{
		const FSlateRect BoundsA = NodeA->GetSubNodeBounds();
		const FSlateRect BoundsB = NodeB->GetSubNodeBounds();
		if (BoundsA.Top == BoundsB.Top)
		{
			return BoundsA.Left < BoundsB.Left;
		}

		return BoundsA.Top < BoundsB.Top;
	};

	NodeSet.StableSort(TopMost);

	for (FBAGroupNode_CommentPtr GroupComment : Comments)
	{
		ApplyCommentY_Recursive(GroupComment->SubNodes, GroupComment->SubComments);
	}

#if DEBUG_COMMENT_Y
	UE_LOG(LogFormatY, Log, TEXT("Processing node set"))
	for (FBAGroupNodePtr Node : NodeSet)
	{
		UE_LOG(LogFormatY, Log, TEXT("\t%s"), *GetNameSafe(Node));
	}
#endif

	// perform collision checks between nodes and push them down
	// since the list is sorted horizontally we can iterate like this
	for (int IndexA = 0; IndexA < NodeSet.Num() - 1; ++IndexA)
	{
		for (int IndexB = IndexA + 1; IndexB < NodeSet.Num(); ++IndexB)
		{
			FBAGroupNodePtr A = NodeSet[IndexA];
			FBAGroupNodePtr B = NodeSet[IndexB];

			if (A->GetSubNodes(true).Num() == 0 && B->GetSubNodes(true).Num() == 0)
			{
				continue;
			}

			if (A->GetBranch() == B->GetBranch())
			{
				continue;
			}

			UE_LOG(LogFormatY, Log, TEXT("Iter %s %s"), *GetNameSafe(A), *GetNameSafe(B))
			FSlateRect BoundsA = A->GetBounds().ExtendBy(FMargin(NodePadding.X, NodePadding.Y));
			FSlateRect BoundsB = B->GetBounds();

#if DEBUG_COMMENT_Y
			GraphHandler->GetGraphOverlay()->DrawBounds(BoundsA, FLinearColor::Blue);
			GraphHandler->GetGraphOverlay()->DrawBounds(BoundsB, FLinearColor::Green);
#endif

			if (FSlateRect::DoRectanglesIntersect(BoundsA, BoundsB))
			{
				const float Delta = BoundsA.Bottom + 1.0f - BoundsB.Top;

				// collect branches from node B (there can be multiple cos it might be a comment wrapping multiple nodes)
				if (FBATreeBranchPtr Branch = B->GetBranch())
				{
					UE_LOG(LogFormatY, Log, TEXT("Moving %s %s"), *GetNameSafe(B), *Branch->ToString());
					Branch->MoveBranchY(Delta);
				}
				else
				{
					UE_LOG(LogFormatY, Log, TEXT("B has no parent branch?"))
					check(false);
				}
			}
		}
	}
}

FBAGroupNode_CommentPtr FBALayout::FindParentComment(const FBAGroupNodePtr& Node, const TSet<FBAGroupNode_CommentPtr>& Comments)
{
	for (FBAGroupNode_CommentPtr Comment : Comments)
	{
		if (Comment->GetSubNodes(true).Contains(Node))
		{
			return Comment;
		}
	}

	return FBAGroupNode_CommentPtr();
}

void FBALayout::PostFormatting()
{
#if 1
	if (!GroupGraph)
	{
		return;
	}

	for (FBAGroupNodePtr Node : GroupGraph->Nodes)
	{
		if (Node->ParentLink.HasBothLinks())
		{
			GraphHandler->GetGraphOverlay()->DrawDebugPinLink("DrawXBranches", Node->ParentLink.AsRegularPinLink(), FLinearColor::MakeRandomColor());
		}

		if (BA_DEBUG("DrawFinalGroups"))
		{
			GraphHandler->GetGraphOverlay()->DrawBounds(Node->GetBounds(), FLinearColor::Green);
		}
	}

	if (BA_DEBUG("DebugTree"))
	{
		Tree.LogTree();
		Tree.DrawTree();
	}

	if (BA_DEBUG("DrawComments"))
	{
		if (auto CommentHandler = GetCommentHandler())
		{
			CommentHandler->DrawBounds(FLinearColor::Yellow);
		}
	}

	MasterGraph->ApplyGraph(INVTEXT("PostFormatting"));
#endif
}

bool FBALayout::IsValidLink(const FBAPinLink& Link)
{
	if (IgnoredNodes.Contains(Link.GetToNode()))
	{
		return false;
	}

	if (RequiredNodes.Num() && !RequiredNodes.Contains(Link.GetToNode()))
	{
		return false;
	}

	if (LinkFilter.IsSet() && !LinkFilter(Link))
	{
		return false;
	}

	return true;
}

TArray<UEdGraphNode*> FBALayout::GetNodePoolAsObjs() const
{
	TArray<UEdGraphNode*> Out;
	Out.Reserve(NodePool.Num());
	Algo::Transform(NodePool, Out, &FBANode::NodeObj);
	return Out;
}

TSet<UEdGraphNode*> FBALayout::GetFormattedNodes()
{
	TSet<UEdGraphNode*> OutNodes;
	for (auto GroupNode : GroupGraph->Nodes)
	{
		Algo::Transform(GroupNode->Nodes, OutNodes, &FBANode::NodeObj);
	}

	return OutNodes;
}
