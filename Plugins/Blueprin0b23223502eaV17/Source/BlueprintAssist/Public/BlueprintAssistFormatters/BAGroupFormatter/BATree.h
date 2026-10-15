// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraph.h"
#include "BAGroupGraph.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"

struct FBATreeBranch;
struct FBATree;

using FBATreeBranchPtr = TSharedPtr<FBATreeBranch>;
struct FBATreeBranch : public TSharedFromThis<FBATreeBranch>
{
	FBATree* Tree;
	FBAGroupLink Link;

	bool bIsRootBranch = false;
	bool bIsAligned = false;
	bool bReturnsToParent = false;
	int32 EntryIndex = -1;
	int BranchIndex = 0;

	FBATreeBranchPtr ParentBranch;

	TArray<FBAGroupNodePtr> BranchNodes;
	TArray<FBATreeBranchPtr> SubBranches;
	FBAGroupNodePtr RootNode;
	FBAGroupNodePtr NodeToAvoid;

	TArray<FBATreeBranchPtr> ChildAvoidY;
	FBATreeBranchPtr ParentAvoidY;

	TSet<FBAGroupLink> CrossLinks;

	FBATreeBranch(FBATree* InTree, FBAGroupNodePtr InRootNode, EEdGraphPinDirection Direction, bool bInIsRootBranch)
		: Tree(InTree)
		, Link(InRootNode, Direction)
		, bIsRootBranch(bInIsRootBranch)
		, RootNode(InRootNode)
	{
	}

	template<typename... Args>
	static FBATreeBranchPtr New(Args&&... InArgs)
	{
		return MakeShared<FBATreeBranch>(Forward<Args>(InArgs)...);
	}

	void SetParent(FBATreeBranchPtr NewParentBranch, const FBAGroupLink& InLink);

	void MoveBranchY(int Delta);

	void MoveBranchWithSpacingY(int Delta);

	void GetVerticallyRelatedBranches(TSet<FBATreeBranchPtr>& OutBranches); 

	void MoveBranchX(int Delta);

	TArray<FSlateRect> GetBoundsForAllBranches();

	TArray<FSlateRect> GetBoundsForNodeArray();

	TArray<FSlateRect> GetWireBounds(TSharedPtr<FBAGraphHandler> GraphHandler, EEdGraphPinDirection Direction, const TSet<FBATreeBranchPtr>& IgnoredBranches);

	FSlateRect GetBranchBounds();

	bool IsSubBranchLink(const FBAGroupLink& PinLink);

	bool IsSubBranchOf(FBATreeBranchPtr Root);

	void ConsumeBranch(FBATreeBranchPtr BranchToConsume);

	void ConsumeNode(const FBAGroupLink& FromLink);

	void AddNode(FBAGroupNodePtr Node);

	void RemoveNode(FBAGroupNodePtr Node);

	void StraightenSubBranches(EEdGraphPinDirection Dir);

	void StraightenBranch(EEdGraphPinDirection Dir);
	void StraightenBranch_Recursive(FBAGroupLink CurrLink, EEdGraphPinDirection Dir, TSet<FBAGroupLink>& Visited, TSet<FBAGroupNodePtr>& Straightened);

	void GetAllSubBranches(TArray<FBATreeBranchPtr>& OutBranches, EEdGraphPinDirection Direction = EGPD_MAX)
	{
		OutBranches.Append(GetSubBranches(Direction));
		for (FBATreeBranchPtr SubBranch : SubBranches)
		{
			if (Direction == EGPD_MAX || SubBranch->Link.GetDirection() == Direction)
			{
				SubBranch->GetAllSubBranches(OutBranches, Direction);
			}
		}
	}

	TArray<FBATreeBranchPtr> GetSubBranches(EEdGraphPinDirection Direction = EGPD_MAX)
	{
		TArray<FBATreeBranchPtr> OutBranches;
		for (FBATreeBranchPtr SubBranch : SubBranches)
		{
			if (Direction == EGPD_MAX || SubBranch->Link.GetDirection() == Direction)
			{
				OutBranches.Add(SubBranch);
			}
		}

		return OutBranches;
	}

	void GetBranches(TArray<FBATreeBranchPtr>& OutBranches, EEdGraphPinDirection Direction = EGPD_MAX)
	{
		OutBranches.Add(AsShared());

		for (FBATreeBranchPtr SubBranch : SubBranches)
		{
			if (Direction == EGPD_MAX || SubBranch->Link.GetDirection() == Direction)
			{
				SubBranch->GetBranches(OutBranches, Direction);
			}
		}
	};

	TArray<FBAGroupNodePtr> GetAllBranchNodes(EEdGraphPinDirection Direction = EGPD_MAX);

	TArray<FBATreeBranchPtr> GetExpandedBranches();
	TArray<FBAGroupNodePtr> GetExpandedNodes();

	TArray<FBANodePtr> GetBranchNodeObjs(EEdGraphPinDirection Direction = EGPD_MAX);

	FBATreeBranchPtr GetParentRootBranch()
	{
		if (bIsRootBranch)
		{
			return AsShared();
		}

		if (ParentBranch.IsValid())
		{
			return ParentBranch->GetParentRootBranch();
		}

		return nullptr;
	}

	void SetAvoidBranchY(FBATreeBranchPtr NewAvoid);

	FString ToString() const;
};

struct BLUEPRINTASSIST_API FBATree : public TSharedFromThis<FBATree>
{
	struct FNewBranchInfo
	{
		TSharedPtr<FBATreeBranch> ParentBranch;
		FBAGroupLink ParentLink;

		FBATreeBranchPtr MakeBranch(FBATree* Tree, FBAGroupNodePtr InRootNode, bool bIsRootBranch, EEdGraphPinDirection Direction);
	};

	EEdGraphPinDirection GraphDirection;
	TArray<FBAGroupNodePtr> TreeNodes;
	TArray<FBATreeBranchPtr> Branches;
	FBAGroupNodePtr RootNode;
	FBATreeBranchPtr RootBranch;

	bool bForceBranchesFromRoot = false;

	TSet<FBAGroupLink> CycleLinks;
	TSet<FBAGroupLink> CrossLinks;
	TArray<FBAGroupLink> LinksToStraighten;

	TFunction<bool(FBAGroupPin&)> PinFilter;

	TSharedPtr<FBAGraphHandler> GraphHandler;

	void MarkCycleLinks();
	void MarkCycleLinks_Recursive(
		FBAGroupNodePtr CurrNode,
		TSet<FBAGroupNodePtr>& Exploring,
		TSet<FBAGroupNodePtr>& Visited);

	void MarkCrossLinks();

	void InitTree(TSharedPtr<FBAGraphHandler> InGraphHandler,
		FBAGroupNodePtr InRootNode,
		EEdGraphPinDirection InDirection);

	void BuildTree(TSharedPtr<FBAGraphHandler> InGraphHandler,
		FBAGroupNodePtr InRootNode,
		EEdGraphPinDirection InDirection);

	void OrderBranches();

	FBAGroupNodePtr FindInputRoot(const FNewBranchInfo& BranchInfo, bool& bOutIsRootNode, TSet<FBAGroupNodePtr>& OutVisited, TSet<FBAGroupNodePtr>& InVisited, TArray<FNewBranchInfo>& PendingBranches);
	FBAGroupNodePtr FindInputRoot_Recursive(const FBAGroupLink& FromLink, bool& bOutIsRootNode, TSet<FBAGroupNodePtr>& OutVisited, TSet<FBAGroupNodePtr>& InVisited, const TSet<FBAGroupLink>& IgnoredPendingLinks);

	void StraightenTree();

	void AddBranch(FBATreeBranchPtr NewBranch)
	{
		int32 Index = Branches.AddUnique(NewBranch);
		NewBranch->BranchIndex = Index;
	}

	TArray<FBATreeBranchPtr> GetRootBranches();

	TArray<FBATreeBranchPtr> GetBranchesFromNode(FBAGroupNodePtr Node);

	EEdGraphPinDirection GetGraphComplementaryDir();

	void Reset();

	bool IsValidPin(FBAGroupPinPtr Pin);
	bool IsValidLink(const FBAGroupLink& Link);

	void LogTree();
	void DrawTree();
};

inline FString GetNameSafe(FBATreeBranchPtr Branch)
{
	if (Branch)
	{
		return Branch->ToString();
	}
	else
	{
		return TEXT("null branch");
	}
}