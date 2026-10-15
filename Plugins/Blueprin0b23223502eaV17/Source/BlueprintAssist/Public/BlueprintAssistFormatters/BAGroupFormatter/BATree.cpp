// Copyright fpwong. All Rights Reserved.


#include "BATree.h"

#include "BAGraphSnapshotManager.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistUtils.h"
#include "BlueprintAssistFormatters/EdGraphFormatter.h"
#include "BlueprintAssistMisc/AlgoBA.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogBuildTree, NoLogging, All);

void FBATreeBranch::SetParent(FBATreeBranchPtr NewParentBranch, const FBAGroupLink& InLink)
{
	if (NewParentBranch == AsShared())
	{
		UE_LOG(LogBATree, Error, TEXT("Error tried to set parent to self? %s"), *ToString());
		return;
	}

	if (ParentBranch)
	{
		ParentBranch->SubBranches.Remove(AsShared());
	}

	ParentBranch = NewParentBranch;
	Link = InLink;

	if (ParentBranch)
	{
		ParentBranch->SubBranches.Add(AsShared());
	}
}

void FBATreeBranch::MoveBranchY(int Delta)
{
	UE_LOG(LogBATree, Log, TEXT("Move branch %s %d"), *ToString(), Delta);
	TArray<FBATreeBranchPtr> Branches;
	GetBranches(Branches);

	for (FBATreeBranchPtr Branch : Branches)
	{
		for (FBAGroupNodePtr BranchNode : Branch->BranchNodes)
		{
			UE_LOG(LogBATree, Log, TEXT("\tMove %s"), *GetNameSafe(BranchNode));
			BranchNode->SetY(BranchNode->NodePosY + Delta);
			// BranchNode->NodePosY = FBAUtils::AlignTo8x8Grid(BranchNode->NodePosY);
			// Formatter->RefreshParameters(BranchNode);
		}
	}
}

void FBATreeBranch::MoveBranchWithSpacingY(int Delta)
{
	TSet<FBATreeBranchPtr> Branches;
	GetVerticallyRelatedBranches(Branches);

	for (auto Branch : Branches)
	{
		Branch->MoveBranchY(Delta);
	}
}

void FBATreeBranch::GetVerticallyRelatedBranches(TSet<FBATreeBranchPtr>& OutBranches)
{
	TArray<FBATreeBranchPtr> PendingBranches = {AsShared()};
	while (!PendingBranches.IsEmpty())
	{
		FBATreeBranchPtr Next = PendingBranches.Pop();

		if (OutBranches.Contains(Next))
		{
			UE_LOG(LogBATree, Error, TEXT("[%hs] Found circular related branches"), __FUNCTION__)
			continue;
		}

		OutBranches.Add(Next);

		PendingBranches.Append(Next->ChildAvoidY);
	}
}

void FBATreeBranch::MoveBranchX(int Delta)
{
	TArray<FBATreeBranchPtr> Branches;
	GetBranches(Branches);

	for (FBATreeBranchPtr Branch : Branches)
	{
		for (FBAGroupNodePtr BranchNode : Branch->BranchNodes)
		{
			// UE_LOG(LogBATree, Log, TEXT("\t%s"), *GetNameSafe(BranchNode));
			BranchNode->SetX(BranchNode->NodePosX + Delta);
			// BranchNode->NodePosY = FBAUtils::AlignTo8x8Grid(BranchNode->NodePosY);
			// Formatter->RefreshParameters(BranchNode);
		}
	}
}

TArray<FSlateRect> FBATreeBranch::GetBoundsForAllBranches()
{
	TArray<FBATreeBranchPtr> Branches;
	GetBranches(Branches);

	TArray<FSlateRect> OutBounds;
	for (FBATreeBranchPtr Branch : Branches)
	{
		OutBounds.Add(Branch->GetBranchBounds());
	}

	return OutBounds;
}

TArray<FSlateRect> FBATreeBranch::GetBoundsForNodeArray()
{
	TArray<FSlateRect> OutBounds;
	for (FBAGroupNodePtr Node : BranchNodes)
	{
		OutBounds.Add(Node->GetBounds());
	}

	return OutBounds;
}

TArray<FSlateRect> FBATreeBranch::GetWireBounds(TSharedPtr<FBAGraphHandler> GraphHandler, EEdGraphPinDirection Direction, const TSet<FBATreeBranchPtr>& IgnoredBranches)
{
	TSet<FBAGroupLink> VisitedLinks;
	TArray<FSlateRect> WireBounds;
	TArray<FBAGroupLink> PendingLinks = {FBAGroupLink(RootNode, Tree->GraphDirection)};
	while (!PendingLinks.IsEmpty())
	{
		const FBAGroupLink& CurrLink = PendingLinks.Pop();

		if (Tree->CycleLinks.Contains(CurrLink))
		{
			continue;
		}

		if (VisitedLinks.Contains(CurrLink))
		{
			continue;
		}

		VisitedLinks.Add(CurrLink);

		if (CurrLink.HasBothLinks())
		{
			if (CurrLink.GetToNode()->GetNodeIndex() - CurrLink.GetFromNode()->GetNodeIndex() == 1)
			{
				FSlateRect Bounds = CurrLink.GetBounds().ExtendBy(FMargin(-2, 0));
				WireBounds.Add(Bounds);
			}
		}

		for (FBAGroupLink NextLink : CurrLink.GetToNode()->GetLinks(Tree->GraphDirection))
		{
			if (BranchNodes.Contains(NextLink.GetToNode()))
			{
				PendingLinks.Add(NextLink);
			}
		}
	}

	return WireBounds;
}

FSlateRect FBATreeBranch::GetBranchBounds()
{
	return FBAUtils::GetGroupedBounds(GetBoundsForNodeArray());
}

void FBATreeBranch::ConsumeBranch(FBATreeBranchPtr BranchToConsume)
{
	for (auto It = BranchToConsume->BranchNodes.CreateIterator(); It; ++It)
	{
		FBAGroupNodePtr Node = *It;
		AddNode(Node);
		Node->Branch = AsShared();
		// Tree.FindOrMakeFormatYInfo(Node)->SetParent(AsShared());
	}

	// if we are consuming our parent branch, we should update our parent to our parent's parent
	if (ParentBranch == BranchToConsume)
	{
		SetParent(BranchToConsume->ParentBranch, BranchToConsume->Link);
		NodeToAvoid = BranchToConsume->NodeToAvoid;
	}

	BranchToConsume->SetParent(nullptr, FBAGroupLink());

	if (BranchToConsume->BranchNodes.Contains(Link.GetFromNode()))
	{
		Link = BranchToConsume->Link;
	}

	Tree->Branches.Remove(BranchToConsume);
}

void FBATreeBranch::ConsumeNode(const FBAGroupLink& FromLink)
{
	BA_DEBUG_EARLY_EXIT("ConsumeNode");

	UE_LOG(LogBuildTree, Log, TEXT("Consume node %s"), *GetNameSafe(FromLink.GetNode()));

	FBAGroupNodePtr NodeToConsume = FromLink.GetToNode();

	// set our parent to the node we are consuming
	SetParent(NodeToConsume->Branch, NodeToConsume->ParentLink);

	AddNode(NodeToConsume);
	NodeToConsume->SetParentLink(FromLink);


	// FromLink.GetFromNode()->ParentBranch;

	// FromLink.GetFromNode()->ParentBranch->AddNode(FromLink.GetFromNode());

	// auto ToNode = FromLink.GetToNode();
	// ToNode->SetParentLink(FromLink);

	// ToNode->ParentBranch->BranchNodes.Remove(ToNode);
	//
	// ToNode->ParentBranch = FromLink.GetFromNode()->ParentBranch;
	// ToNode->ParentBranch->BranchNodes.Add(ToNode);

	// // TODO
	// UE_LOG(LogTemp, Warning, TEXT("Consume node %s"), *GetNameSafe(Node));
	//
	// Node->SetParentLink()
}

void FBATreeBranch::AddNode(FBAGroupNodePtr Node)
{
	if (BranchNodes.Contains(Node))
	{
		return;
	}

	if (Node->Branch)
	{
		Node->Branch->RemoveNode(Node);
	}

	UE_LOG(LogBuildTree, Log, TEXT("Add %s to %s"), *GetNameSafe(Node), *ToString());
	BranchNodes.Add(Node);
	Node->Branch = AsShared();
}

void FBATreeBranch::RemoveNode(FBAGroupNodePtr Node)
{
	BranchNodes.Remove(Node);
	Node->Branch.Reset();
}

void FBATreeBranch::StraightenSubBranches(EEdGraphPinDirection Dir)
{
	FBAScopedSnapshotGroup G(FString::Printf(TEXT("Straighten Sub Branch %s"), *ToString()), RootNode->Graph);
	StraightenBranch(Dir);
	FBAScopedSnapshotGroup::MakeLeafSnapshot(FString::Printf(TEXT("Straightened %s"), *ToString()), RootNode->Graph);

	if (Link.HasBothLinks())
	{
		float StraightenDist = Link.CalcStraightenDist();
		MoveBranchY(StraightenDist);

		FBAScopedSnapshotGroup::MakeLeafSnapshot(
			FString::Printf(TEXT("Aligned %s"), *ToString()), 
			RootNode->Graph);
	}

	for (FBATreeBranchPtr SubBranch : SubBranches)
	{
		if (SubBranch->Link.GetDirection() == Dir)
		{
			SubBranch->StraightenSubBranches(Dir);
		}
	}
}

void FBATreeBranch::StraightenBranch(EEdGraphPinDirection Dir)
{
	TSet<FBAGroupLink> Visited;
	TSet<FBAGroupNodePtr> Straightened;
	// FBAGroupLink RootLink = Link.HasBothLinks() ? Link : FBAGroupLink(RootNode, Dir);
	StraightenBranch_Recursive(FBAGroupLink(RootNode, Dir), Dir, Visited, Straightened); 
}

void FBATreeBranch::StraightenBranch_Recursive(
	FBAGroupLink CurrLink,
	EEdGraphPinDirection Dir,
	TSet<FBAGroupLink>& Visited,
	TSet<FBAGroupNodePtr>& Straightened)
{
	if (Visited.Contains(CurrLink) || Visited.Contains(CurrLink.Opposite()))
	{
		return;
	}

	Visited.Add(CurrLink);

	if (Tree->CycleLinks.Contains(CurrLink))
	{
		return;
	}

	FBAGroupNodePtr CurrNode = CurrLink.GetToNode();

	if (CurrNode->Branch != AsShared())
	{
		return;
	}

	if (!Straightened.Contains(CurrNode))
	{
		Straightened.Add(CurrNode);

		if (CurrLink.HasBothLinks())
		{
			if (BA_DEBUG("TestAlignStyle")) // TODO Make setting for this
			{
				CurrLink.GetToNode()->SetY(CurrLink.GetFromNode()->GetY());
			}
			else
			{
				CurrLink.Straighten();
			}

			// FBAScopedSnapshotGroup::MakeLeafSnapshot(CurrLink.ToString(), RootNode->Graph);
			UE_LOG(LogBATree, Log, TEXT("\tStraighten %s"), *CurrLink.ToString());
		}
	}

	for (FBAGroupLink NextLink : CurrNode->GetLinks(Dir))
	{
		StraightenBranch_Recursive(NextLink, Dir, Visited, Straightened);
	}
}

TArray<FBAGroupNodePtr> FBATreeBranch::GetAllBranchNodes(EEdGraphPinDirection Direction)
{
	TArray<FBATreeBranchPtr> Branches;
	GetBranches(Branches, Direction);

	TArray<FBAGroupNodePtr> OutNodes;
	AlgoBA::TransformAccumulate(Branches, OutNodes, &FBATreeBranch::BranchNodes, TAppend());

	return OutNodes;
}

TArray<FBATreeBranchPtr> FBATreeBranch::GetExpandedBranches()
{
	TArray<FBATreeBranchPtr> OutBranches;

	TQueue<FBATreeBranchPtr> Pending;
	Pending.Enqueue(AsShared());
	while (!Pending.IsEmpty())
	{
		FBATreeBranchPtr Branch;
		Pending.Dequeue(Branch);

		OutBranches.Add(Branch);

		for (auto Sub : Branch->SubBranches)
		{
			if (Sub->bReturnsToParent)
			{
				Pending.Enqueue(Sub);
			}
		}
	}

	return OutBranches;
}

TArray<FBAGroupNodePtr> FBATreeBranch::GetExpandedNodes()
{
	TArray<FBAGroupNodePtr> OutNodes;

	TQueue<FBATreeBranchPtr> Pending;
	Pending.Enqueue(AsShared());
	while (!Pending.IsEmpty())
	{
		FBATreeBranchPtr Branch;
		Pending.Dequeue(Branch);

		OutNodes.Append(Branch->BranchNodes);

		for (auto Sub : Branch->SubBranches)
		{
			if (Sub->bReturnsToParent)
			{
				Pending.Enqueue(Sub);
			}
		}
	}

	return OutNodes;
}

TArray<FBANodePtr> FBATreeBranch::GetBranchNodeObjs(EEdGraphPinDirection Direction)
{
	TArray<FBANodePtr> AllNodes;
	AlgoBA::TransformAccumulate(BranchNodes, AllNodes, &FBAGroupNode::Nodes, TAppend());
	return AllNodes;
}

bool FBATreeBranch::IsSubBranchLink(const FBAGroupLink& PinLink)
{
	for (auto SubBranch : SubBranches)
	{
		if (SubBranch->Link == PinLink)
		{
			return true;
		}
	}

	return false;
}

bool FBATreeBranch::IsSubBranchOf(FBATreeBranchPtr Root)
{
	FBATreeBranchPtr Curr = AsShared();
	while (Curr)
	{
		if (Curr == Root)
		{
			return true;
		}

		Curr = Curr->ParentBranch;
	}

	return false;
}

void FBATreeBranch::SetAvoidBranchY(FBATreeBranchPtr NewAvoid)
{
	if (ParentAvoidY)
	{
		ParentAvoidY->ChildAvoidY.Remove(AsShared());
	}

	ParentAvoidY = NewAvoid;

	if (NewAvoid)
	{
		NewAvoid->ChildAvoidY.Add(AsShared());
	}
}

FString FBATreeBranch::ToString() const
{
	FString BranchType = bIsRootBranch ? "Root" : "Sub";
	// return FString::Printf(TEXT("%s | %s %s | %d %p"), *BranchType, *GetNameSafe(RootNode), *Link.ToString(), BranchNodes.Num(), this);
	return FString::Printf(TEXT("Branch <%s %s (%d %p)>"), *BranchType, *GetNameSafe(RootNode), BranchNodes.Num(), this);
}

FBATreeBranchPtr FBATree::FNewBranchInfo::MakeBranch(FBATree* Tree, FBAGroupNodePtr InRootNode, bool bIsRootBranch, EEdGraphPinDirection Direction)
{
	UE_LOG(LogBuildTree, Log, TEXT("Making new branch ROOT %s PAR %s LINK %s"), *GetNameSafe(InRootNode), *GetNameSafe(ParentBranch), *ParentLink.ToString());
	FBATreeBranchPtr NewBranch = FBATreeBranch::New(Tree, InRootNode, Direction, bIsRootBranch);
	NewBranch->SetParent(ParentBranch, ParentLink);
	return NewBranch;
}

TArray<FBATreeBranchPtr> FBATree::GetRootBranches()
{
	return Branches.FilterByPredicate(&FBATreeBranch::bIsRootBranch);
}

TArray<FBATreeBranchPtr> FBATree::GetBranchesFromNode(FBAGroupNodePtr Node)
{
	TArray<FBATreeBranchPtr> OutBranches;

	if (Node)
	{
		if (FBATreeBranchPtr NodeBranch = Node->GetBranch())
		{
			for (FBATreeBranchPtr SubBranch : NodeBranch->SubBranches)
			{
				if (SubBranch->Link.GetFromNode() == Node)
				{
					OutBranches.Add(SubBranch);
				}
			}
		}
	}

	return OutBranches;
}

void FBATree::MarkCycleLinks()
{
	CycleLinks.Reset();

	TSet<FBAGroupNodePtr> Visited, Path;
	for (FBAGroupNodePtr Node : RootNode->Graph->Nodes)
	{
		if (!Visited.Contains(Node))
		{
			MarkCycleLinks_Recursive(Node, Path, Visited);
		}
	}
}

void FBATree::MarkCycleLinks_Recursive(FBAGroupNodePtr CurrNode, TSet<FBAGroupNodePtr>& Exploring, TSet<FBAGroupNodePtr>& Visited)
{
	UE_LOG(LogBATree, Log, TEXT("[%hs] Iter %s"), __FUNCTION__, *GetNameSafe(CurrNode));
	Exploring.Add(CurrNode);

	TArray<FBAGroupLink> OutgoingLinks = CurrNode->GetLinks(GraphDirection);
	for (FBAGroupLink& Link : OutgoingLinks)
	{
		FBAGroupNodePtr ToNode = Link.GetToNode();
		if (Exploring.Contains(ToNode))
		{
			UE_LOG(LogBATree, Log, TEXT("[%hs] Add cycle link %s"), __FUNCTION__, *Link.ToString());
			CycleLinks.Add(Link);
			CycleLinks.Add(Link.Opposite());
		}
		else if (!Visited.Contains(ToNode))
		{
			MarkCycleLinks_Recursive(ToNode, Exploring, Visited);
		}
	}

	Exploring.Remove(CurrNode);
	Visited.Add(CurrNode);
}

void FBATree::MarkCrossLinks()
{
	for (auto Branch : Branches)
	{
		for (FBAGroupNodePtr Node : Branch->BranchNodes)
		{
			UE_LOG(LogBATree, Log, TEXT("[%hs] Iter Node %s"), __FUNCTION__, *GetNameSafe(Node));
			for (FBAGroupLink Link : Node->GetLinks())
			{
				// if they are from different root branches then they are cross links
				FBATreeBranchPtr FromBranch = Link.GetFromNode()->GetBranch();
				FBATreeBranchPtr ToBranch = Link.GetToNode()->GetBranch();

				UE_LOG(LogBATree, Log, TEXT("[%hs] Checking cross link %s (%s -> %s)"), __FUNCTION__, *Link.ToString(), *GetNameSafe(FromBranch), *GetNameSafe(ToBranch));

				if (CycleLinks.Contains(Link))
				{
					UE_LOG(LogBATree, Warning, TEXT("[%hs] \tSkip cycle"), __FUNCTION__);
					continue;
				}

				if (FromBranch->GetParentRootBranch() != ToBranch->GetParentRootBranch())
				{
					UE_LOG(LogBATree, Log, TEXT("[%hs] Add cross link %s"), __FUNCTION__, *Link.ToString());
					CrossLinks.Add(Link);
					CrossLinks.Add(Link.Opposite());

					FromBranch->CrossLinks.Add(Link);
					ToBranch->CrossLinks.Add(Link.Opposite());
				}
				
			}
		}
	}
}

void FBATree::InitTree(TSharedPtr<FBAGraphHandler> InGraphHandler, FBAGroupNodePtr InRootNode, EEdGraphPinDirection InDirection)
{
	TreeNodes.Reset();
	Branches.Reset();

	RootNode = InRootNode;

	GraphHandler = InGraphHandler;
	GraphDirection = InDirection;
	PinFilter = [](FBAGroupPin& Pin) { return true; };
}

void FBATree::BuildTree(TSharedPtr<FBAGraphHandler> InGraphHandler,
		FBAGroupNodePtr InRootNode,
		EEdGraphPinDirection InDirection)
{
	InitTree(InGraphHandler, InRootNode, InDirection);

	MarkCycleLinks();

	FBADrawBuilder Draw;

	TSet<FBAGroupNodePtr> Visited;

	TArray<FBAGroupNodePtr> CurrStack = { RootNode };
	TArray<FNewBranchInfo> NextRows;
	TQueue<FNewBranchInfo> NextInputRows;

	TSharedPtr<FBATreeBranch> CurrBranch = FBATreeBranch::New(this, RootNode, GraphDirection, true);
	RootBranch = CurrBranch;

	while (!CurrStack.IsEmpty() || !NextRows.IsEmpty() || !NextInputRows.IsEmpty())
	{
		FBAGroupNodePtr Next = nullptr;

		// the curr stack iters the node in the curr row
		if (!CurrStack.IsEmpty())
		{
			Next = CurrStack.Pop();
			UE_LOG(LogBuildTree, Log, TEXT("[%hs] Iter next %s"), __FUNCTION__, *GetNameSafe(Next));

			Draw.Add(MakeShared<FBADrawNodeAction>(Next->Nodes[0]->NodeObj, GraphHandler->GetGraphPanel()));
		}
		// use the next pending row
		else if (!NextRows.IsEmpty())
		{
			FNewBranchInfo BranchInfo = NextRows.Pop();

			UE_LOG(LogBuildTree, Log, TEXT("[%hs] Finding output root for %s"), __FUNCTION__, *GetNameSafe(BranchInfo.ParentLink.GetToNode()));

			TSet<FBAGroupNodePtr> TmpVisited;
			bool bIsRootBranch = false;

			FBATreeBranchPtr ExistingBranch = BranchInfo.ParentLink.GetToNode()->GetBranch();

			Draw.Add(MakeShared<FBADrawNodeAction>(BranchInfo.ParentLink.GetToNode()->Nodes[0]->NodeObj, GraphHandler->GetGraphPanel(), FLinearColor::Yellow));

			// find the branch for the next node if it doesn't have one
			if (!ExistingBranch.IsValid())
			{
				if (FBAGroupNodePtr NextRoot = FindInputRoot(BranchInfo, bIsRootBranch, Visited, TmpVisited, NextRows))
				{
					if (!Visited.Contains(NextRoot))
					{
						UE_LOG(LogBuildTree, Log, TEXT("[%hs] Iter new output row %s from %s (%d)"), __FUNCTION__, *GetNameSafe(NextRoot), *CurrBranch->Link.ToString(), bIsRootBranch);
						CurrBranch = BranchInfo.MakeBranch(this, NextRoot, bIsRootBranch, GraphDirection);
						CurrStack.Push(NextRoot);
					}
				}
			}
			else // check if our link is better for this branch
			{
				// the link is better if from link is earlier on the branch than our current link 
				FBAGroupNodePtr ExistingFromNode = ExistingBranch->Link.GetFromNode();
				FBAGroupNodePtr NewFromNode = BranchInfo.ParentLink.GetFromNode();

				if (ExistingFromNode->Branch == NewFromNode->Branch)
				{
					if (NewFromNode->GetNodeIndex() < ExistingFromNode->GetNodeIndex())
					{
						UE_LOG(LogBuildTree, Log, TEXT("[%hs] Took new link %s for %s"), __FUNCTION__, *BranchInfo.ParentLink.ToString(), *ExistingBranch->ToString());
						ExistingBranch->Link = BranchInfo.ParentLink;
					}
				}
			}
			// else
			// {
			// 	UE_LOG(LogBuildTree, Error, TEXT("Failed to find output root?"))
			// }

			continue;
		}
		// use the next pending input row (need to iterate backwards to find the true root)
		else if (!NextInputRows.IsEmpty())
		{
			FNewBranchInfo BranchInfo;
			NextInputRows.Dequeue(BranchInfo);

			FBAGroupNodePtr ToNode = BranchInfo.ParentLink.GetToNode();
			if (Visited.Contains(ToNode))
			{
				UE_LOG(LogBuildTree, Log, TEXT("Checking input branch visited"));

				// check if this link should be used as the parent link
				FBATreeBranchPtr NextBranch = ToNode->GetBranch();
				FBATreeBranchPtr CurrParBranch = NextBranch->ParentBranch;

				FBATreeBranchPtr NewParBranch = BranchInfo.ParentLink.GetFromNode()->GetBranch();
				if (CurrParBranch == NewParBranch)
				{
					int CurrentParIndex = NextBranch->Link.GetFromNode()->GetNodeIndex();
					int NewParIndex = BranchInfo.ParentLink.GetFromNode()->GetNodeIndex();
					UE_LOG(LogBuildTree, Log, TEXT("Comparing indexes %d %d"), CurrentParIndex, NewParIndex);

					if (NewParIndex < CurrentParIndex)
					{
						LinksToStraighten.Remove(NextBranch->Link);

						UE_LOG(LogBuildTree, Log, TEXT("Took better parent link"))
						NextBranch->SetParent(NewParBranch, BranchInfo.ParentLink);

						LinksToStraighten.Add(BranchInfo.ParentLink);
					}
				}

				continue;
			}

			UE_LOG(LogBuildTree, Log, TEXT("[%hs] Finding input root for %s"), __FUNCTION__, *GetNameSafe(BranchInfo.ParentLink.GetToNode()));

			TSet<FBAGroupNodePtr> TmpVisited;
			bool bIsRootNode = false;
			if (FBAGroupNodePtr NextRoot = FindInputRoot(BranchInfo, bIsRootNode, Visited, TmpVisited, NextRows))
			{
				if (!Visited.Contains(NextRoot))
				{
					UE_LOG(LogBuildTree, Log, TEXT("[%hs] Iter new input row %s from %s (%d)"), __FUNCTION__, *GetNameSafe(NextRoot), *CurrBranch->Link.ToString(), bIsRootNode);
					CurrBranch = BranchInfo.MakeBranch(this, NextRoot, bIsRootNode, GraphDirection);
					CurrStack.Push(NextRoot);

					LinksToStraighten.Add(BranchInfo.ParentLink);
				}
			}
			else
			{
				UE_LOG(LogBuildTree, Error, TEXT("Failed to find input root?"))
			}

			continue;
		}

		if (Visited.Contains(Next))
		{
			continue;
		}

		Visited.Add(Next);

		UE_LOG(LogBuildTree, Log, TEXT("[%hs] Iterate next %s (%s)"), __FUNCTION__, *GetNameSafe(Next), *GetNameSafe(CurrBranch));

		CurrBranch->AddNode(Next);

		TArray<FBAGroupLink> FilteredLinks;
		TArray<FBAGroupLink> Links = Next->GetLinks();
		for (const FBAGroupLink& Link : Links)
		{
			FBAGroupNodePtr ToNode = Link.GetToNode();

			// UE_LOG(LogBuildTree, Log, TEXT("Checking %d %s %s"), i, *GetNameSafe(Next), *GetNameSafe(ToNode));

			if (Visited.Contains(ToNode))
			{
				if (Link.GetDirection() == GraphDirection)
				{
					if (ToNode->GetBranch() == CurrBranch->ParentBranch)
					{
						CurrBranch->bReturnsToParent = true;
					}
				}

				continue;
			}

			if (CycleLinks.Contains(Link))
			{
				continue;
			}

			if (CurrStack.Contains(ToNode))
			{
				continue;
			}

			FilteredLinks.Add(Link);
		}

		FilteredLinks.StableSort([&](const FBAGroupLink& A, const FBAGroupLink& B)
		{
			if (A.GetDirection() != B.GetDirection())
			{
				// graph direction first
				return (A.GetDirection() == GraphDirection) > (B.GetDirection() == GraphDirection);
			}

			if (A.From == B.From)
			{
				return A.To->GetPos().Y < B.To->GetPos().Y; 
			}

			return A.From->CachedPinOffset < B.From->CachedPinOffset;
		});

		// UE_LOG(LogBuildTree, Log, TEXT("[%hs] LOG FILTERED LINKS %s"), __FUNCTION__, *GetNameSafe(Next));
		// for (int i = FilteredLinks.Num() - 1; i >= 0; --i)
		// {
		// 	FPinLink& Link = FilteredLinks[i];
		// 	UE_LOG(LogBuildTree, Log, TEXT("[%hs] FilteredLink Order %s"), __FUNCTION__, *Link.ToString());
		// }

		for (int i = 0; i < FilteredLinks.Num(); ++i)
		{
			auto& Link = FilteredLinks[i];

			FBAGroupNodePtr ToNode = Link.GetToNode();
			if (Visited.Contains(ToNode))
			{
				continue;
			}

			UE_LOG(LogBuildTree, Log, TEXT("[%hs] Link %s"), __FUNCTION__, *Link.ToString());

			if (Link.GetDirection() == GraphDirection)
			{
				bool bForceNewBranch = bForceBranchesFromRoot && (Link.GetFromNode() == RootNode || CurrBranch->bReturnsToParent);

				// 1. same row if it's the first pin (since we sorted by direction earlier, 0 will always be the first output pin)
				// 2. make a new branch if we return to our parent 
				if (i == 0 && !bForceNewBranch)
				{
					UE_LOG(LogBuildTree, Log, TEXT("[%hs] Add curr row %s"), __FUNCTION__, *Link.ToString());
					CurrStack.Push(ToNode);
				}
				else // make a new row
				{
					UE_LOG(LogBuildTree, Log, TEXT("[%hs] Add output row %s (%d) (Force: %d)"), __FUNCTION__, *Link.ToString(), CurrBranch->bReturnsToParent, bForceNewBranch);
					FNewBranchInfo NewRowInfo(CurrBranch, Link);
					NextRows.Push(NewRowInfo);
				}
			}
			else
			{
				UE_LOG(LogBuildTree, Log, TEXT("[%hs] Add input row %s"), __FUNCTION__, *Link.ToString());
				FNewBranchInfo NewRowInfo(CurrBranch, Link);
				NextInputRows.Enqueue(NewRowInfo);
			}
		}
	}

	if (BA_DEBUG("DebugBuildTree"))
	{
		if (auto Overlay = GraphHandler->GetGraphOverlay())
		{
			static uint32 Hash = GetTypeHash(FName("DebugBuildTree"));
			if (!Overlay->HasDrawAction(Hash))
			{
				Overlay->AddDrawAction(Draw.Queue(1.0f), Hash);
			}
		}
	}

	OrderBranches();

	MarkCrossLinks();
}

void FBATree::OrderBranches()
{
	// add the branches in correct order for format-y
	TArray<FBATreeBranchPtr> RowStack{RootBranch};
	while (RowStack.Num() > 0)
	{
		FBATreeBranchPtr CurrRow = RowStack.Pop();
		AddBranch(CurrRow);

		UE_LOG(LogBuildTree, Log, TEXT("Order Branch %s"), *CurrRow->ToString());

		for (EEdGraphPinDirection Dir : {FBAUtils::GetOppositeDirection(GraphDirection), GraphDirection})
		{
			TArray<FBATreeBranchPtr> SubBranches = CurrRow->GetSubBranches(Dir);
			SubBranches.StableSort([&](const FBATreeBranchPtr& A, const FBATreeBranchPtr& B)
			{
				// left most first
				int NodeIndexA = A->Link.GetFromNode()->GetNodeIndex();
				int NodeIndexB = B->Link.GetFromNode()->GetNodeIndex();
				if (NodeIndexA != NodeIndexB)
				{
					return NodeIndexA < NodeIndexB;
				}

				// sort by bottom pin
				const int PinIndexA = A->Link.From->PinIndex;
				const int PinIndexB = B->Link.From->PinIndex;
				if (PinIndexA != PinIndexB)
				{
					return PinIndexA > PinIndexB;
				}

				return true;
			});

			for (FBATreeBranchPtr Branch : SubBranches)
			{
				UE_LOG(LogBuildTree, Log, TEXT("\tSubBranch %s"), *Branch->ToString());
				RowStack.Push(Branch);
			}
		}
	}

	for (auto Branch : Branches)
	{
		Branch->SubBranches.StableSort([&](const FBATreeBranchPtr& A, const FBATreeBranchPtr& B)
		{
			return A->BranchIndex < B->BranchIndex;
		});
	}
}

FBAGroupNodePtr FBATree::FindInputRoot(const FNewBranchInfo& BranchInfo, bool& bOutIsRootNode, TSet<FBAGroupNodePtr>& OutVisited, TSet<FBAGroupNodePtr>& InVisited, TArray<FNewBranchInfo>& PendingBranches)
{
	FBAGroupNodePtr Node = BranchInfo.ParentLink.GetToNode();

	UE_LOG(LogBuildTree, Warning, TEXT("[%hs] Finding input root for %s"), __FUNCTION__, *Node->NodeName);

	// if the branch returns to parent, then simply stop here
	bool bReturnsToParent = false;
	TArray<FBAGroupLink> Stack;
	Stack.Add(BranchInfo.ParentLink);

	TSet<FBAGroupLink> StackVisited;
	while (!Stack.IsEmpty())
	{
		FBAGroupLink Link = Stack.Pop();
		FBAGroupNodePtr ToNode = Link.GetNode();

		if (CycleLinks.Contains(Link))
		{
			continue;
		}
		
		if (StackVisited.Contains(Link))
		{
			continue;
		}

		StackVisited.Add(Link);

		if (OutVisited.Contains(ToNode))
		{
			if (!(Link <=> BranchInfo.ParentLink.Opposite()))
			{
				UE_LOG(LogBuildTree, Warning, TEXT("[%hs] Branch is linked to parent, using %s as root"), __FUNCTION__, *Node->NodeName);
				bReturnsToParent = true;
				break;
			}

			continue;
		}

		Stack.Append(ToNode->GetLinks(GraphDirection));
	}

	if (bReturnsToParent)
	{
		return Node;
	}

	if (bForceBranchesFromRoot)
	{
		if (BranchInfo.ParentLink.GetFromNode() == RootNode)
		{
			return Node;
		}
	}

	// if we find that this branch hits any pending branches, we should exit and not iterate this branch 
	TSet<FBAGroupLink> IgnoredPendingLinks;
	Algo::Transform(PendingBranches, IgnoredPendingLinks, &FNewBranchInfo::ParentLink);

	return FindInputRoot_Recursive(BranchInfo.ParentLink, bOutIsRootNode, OutVisited, InVisited, IgnoredPendingLinks);
}

FBAGroupNodePtr FBATree::FindInputRoot_Recursive(const FBAGroupLink& FromLink, bool& bOutIsRootNode, TSet<FBAGroupNodePtr>& OutVisited, TSet<FBAGroupNodePtr>& InVisited, const TSet<FBAGroupLink>& IgnoredPendingLinks)
{
	FBAGroupNodePtr Node = FromLink.GetToNode();

	// already visited, skip
	if (Node->GetBranch())
	{
		UE_LOG(LogBuildTree, Warning, TEXT("[%hs] %s Already has parent, skipping"), __FUNCTION__, *GetNameSafe(Node));
		return nullptr;
	}

	if (InVisited.Contains(Node))
	{
		UE_LOG(LogBuildTree, Warning, TEXT("[%hs] %s InVisited contains, skipping"), __FUNCTION__, *GetNameSafe(Node));
		return nullptr;
	}

	InVisited.Add(Node);

	// GraphHandler->GetGraphOverlay()->DrawNodeInQueue(Node);
	UE_LOG(LogBuildTree, Log, TEXT("[%hs] %s"), __FUNCTION__, *GetNameSafe(Node));

	TArray<FBAGroupLink> Links = Node->GetLinks(FBAUtils::GetOppositeDirection(GraphDirection));

	bool bIsRootNode = true;

	TArray<FBAGroupLink> FilteredInputLinks;
	for (auto& Link : Links)
	{
		if (OutVisited.Contains(Link.GetToNode()))
		{
			bIsRootNode = false;

			// exit condition to skip this branch when we hit another pending branch
			if (IgnoredPendingLinks.Contains(Link.Opposite()))
			{
				UE_LOG(LogBuildTree, Warning, TEXT("Ignoring new parent branch due to link %s"), *Link.Opposite().ToString());
				return nullptr;
			}

			continue;
		}

		if (!CycleLinks.Contains(Link))
		{
			FilteredInputLinks.Add(Link);
		}
	}

	// root has no exec input links
	if (FilteredInputLinks.Num() == 0)
	{
		if (bIsRootNode)
		{
			bOutIsRootNode = true;
		}

		UE_LOG(LogBuildTree, Log, TEXT("[%hs] No input links, returning %s (root %d)"), __FUNCTION__, *GetNameSafe(Node), bOutIsRootNode);

		return Node;
	}

	for (FBAGroupLink& Link : FilteredInputLinks)
	{
		if (FBAGroupNodePtr Root = FindInputRoot_Recursive(Link, bOutIsRootNode, OutVisited, InVisited, IgnoredPendingLinks))
		{
			UE_LOG(LogBuildTree, Log, TEXT("[%hs] FOUND ROOT %s"), __FUNCTION__, *GetNameSafe(Root));
			return Root;
		}
	}

	return nullptr;
}

void FBATree::StraightenTree()
{
	TSet<FBAGroupLink> VisitedLinks;
	TSet<FBAGroupNodePtr> Straightened;

	// straighten all the root branches
	{
		FBAScopedSnapshotGroup G1("Straighten All Branches", RootNode->Graph);
		for (FBATreeBranchPtr Branch : Branches)
		{
			if (!Branch->bIsRootBranch)
			{
				continue;
			}

			FBAScopedSnapshotGroup G2(FString::Printf(TEXT("Branch %s"), *Branch->ToString()), RootNode->Graph);
			UE_LOG(LogFormatY, Log, TEXT("Straighten Root Branch %s"), *Branch->ToString());
			Branch->StraightenSubBranches(GraphDirection);
		}
	}

	{
		FBAScopedSnapshotGroup G2("Straighten Cross Links", RootNode->Graph);
		for (const FBAGroupLink& Link : LinksToStraighten)
		{
			FBATreeBranchPtr ToBranch = Link.GetToNode()->GetBranch()->GetParentRootBranch();
			float StraightenDist = Link.CalcStraightenDist();
			ToBranch->MoveBranchY(StraightenDist);
			FBAScopedSnapshotGroup::MakeLeafSnapshot("Straighten Cross Link", RootNode->Graph);
			UE_LOG(LogFormatY, Log, TEXT("Straighten Cross Link %s: %s Dist: %f"), *Link.ToString(), *ToBranch->ToString(), StraightenDist);
		}
	}
}

EEdGraphPinDirection FBATree::GetGraphComplementaryDir()
{
	return UEdGraphPin::GetComplementaryDirection(GraphDirection);
}

void FBATree::Reset()
{
	CycleLinks.Empty();
	Branches.Empty();
}

bool FBATree::IsValidPin(FBAGroupPinPtr Pin)
{
	return Pin && PinFilter(*Pin.Get());
}

bool FBATree::IsValidLink(const FBAGroupLink& Link)
{
	if (Link.From)
	{
		return IsValidPin(Link.From);
	}

	if (Link.FallbackNode)
	{
		return true;
	}

	return false;
}

void FBATree::LogTree()
{
	UE_LOG(LogBuildTree, Log, TEXT("===BEGIN LOG TREE==="));
	UE_LOG(LogBuildTree, Log, TEXT("BRANCHES %d ROOT %d"), Branches.Num(), GetRootBranches().Num());
	for (auto& Branch : Branches)
	{
		FLinearColor BranchColour = FLinearColor::MakeRandomColor();
		FSlateRect Bounds = Branch->GetBranchBounds();
		GraphHandler->GetGraphOverlay()->DrawBounds(Bounds, BranchColour);

		if (Branch->Link.From.IsValid() && Branch->Link.To.IsValid())
		{
			FPinLink Link(Branch->Link.From->PinHandle->PinHandle.GetPin(), Branch->Link.To->PinHandle->PinHandle.GetPin());
			GraphHandler->GetGraphOverlay()->DrawDebugPinLink("DrawXBranches", Link, BranchColour);
		}

		UE_LOG(LogBuildTree, Log, TEXT("%s"), *Branch->ToString());
		UE_LOG(LogBuildTree, Log, TEXT("\tBRANCH NODES"))
		for (FBAGroupNodePtr Node : Branch->BranchNodes)
		{
			UE_LOG(LogBuildTree, Log, TEXT("\t\t%s"), *GetNameSafe(Node));
		}

		UE_LOG(LogBuildTree, Log, TEXT("\tSUB BRANCHES"))
		for (auto Sub : Branch->SubBranches)
		{
			UE_LOG(LogBuildTree, Log, TEXT("\t\t%s"), *Sub->ToString());
		}
	}
	
	UE_LOG(LogBuildTree, Log, TEXT("PARENTS"))
	if (RootNode)
	{
		for (auto Node : RootNode->Graph->Nodes)
		{
			UE_LOG(LogBuildTree, Log, TEXT("\t%s %s"), *GetNameSafe(Node), *GetNameSafe(Node->Branch));
		}
	}
	UE_LOG(LogBuildTree, Log, TEXT("===ENDED LOG TREE==="));
}

void FBATree::DrawTree()
{
#if 1
	int i = 0;
	for (auto& Branch : Branches)
	{
		i += 1;
		UE_LOG(LogBATree, Log, TEXT("Branch %d: %s"), i, *Branch->ToString())
		int j = 0;
		for (auto BranchNode : Branch->BranchNodes)
		{
			j += 1;
			FBAGraphOverlayTextParams TextParams;
			TextParams.Text = FText::FromString(FString::Printf(TEXT("%d.%d"), i, j));
			UE_LOG(LogBATree, Log, TEXT("\tNode %d: %s"), j, *Branch->ToString())

			for (auto Node : BranchNode->Nodes)
			{
				TSharedPtr<SGraphNode> GraphNode = GraphHandler->GetGraphNode(Node->NodeObj);
				TextParams.Widget = GraphNode;
				TextParams.WidgetBounds = FBAUtils::GetNodeBounds(GraphNode);
				TextParams.TimeRemaining = 5.0f;
				GraphHandler->GetGraphOverlay()->DrawTextOverWidget(TextParams);
			}
		}

		if (BA_DEBUG("DrawTreeEdges"))
		{
			if (Branch->Link.HasBothLinks())
			{
				GraphHandler->GetGraphOverlay()->DrawLine(Branch->Link.From->GetPos(), Branch->Link.To->GetPos(), FLinearColor::Blue);
			}

			for (const auto& Link : CrossLinks)
			{
				GraphHandler->GetGraphOverlay()->DrawLine(Link.From->GetPos(), Link.To->GetPos(), FLinearColor::Yellow);
			}

			for (const auto& Link : CycleLinks)
			{
				GraphHandler->GetGraphOverlay()->DrawLine(Link.From->GetPos(), Link.To->GetPos(), FLinearColor::Red);
			}

			if (Branch->bIsRootBranch)
			{
				GraphHandler->GetGraphOverlay()->DrawBounds(Branch->BranchNodes[0]->GetBounds(), FLinearColor::Green);
			}
		}
	}
#endif
}
