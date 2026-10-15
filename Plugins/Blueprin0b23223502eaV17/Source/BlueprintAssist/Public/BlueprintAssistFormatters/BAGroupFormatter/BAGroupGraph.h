// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraph.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"

class FBAGraphHandler;
struct FBACommentContainsNode;
class UEdGraphNode;
struct FBAGroupPin;
struct FBAGroupGraph;
struct FBAGroupNode;
struct FBATreeBranch;

using FBAGroupPinPtr = TSharedPtr<FBAGroupPin>;
using FBAGroupNodePtr = TSharedPtr<FBAGroupNode>;
using FBAGroupGraphPtr = TSharedPtr<FBAGroupGraph>;
using FBATreeBranchPtr = TSharedPtr<FBATreeBranch>;

// ======================================
// PinLink
// ======================================

struct FBAGroupLink
{
	FBAGroupPinPtr From;
	FBAGroupPinPtr To;

	// sometimes we use a fallback node and a direction to kickstart iteration
	FBAGroupNodePtr FallbackNode;
	EEdGraphPinDirection DirectionOverride = EGPD_MAX;

	FBAGroupLink() {}
	FBAGroupLink(const FBAGroupPinPtr& InFrom, const FBAGroupPinPtr& InTo) : From(InFrom), To(InTo) {}
	FBAGroupLink(const FBAGroupNodePtr& InNode, const EEdGraphPinDirection& InDirection) : FallbackNode(InNode), DirectionOverride(InDirection) {}

	EEdGraphPinDirection GetDirection() const;
	static TFunction<bool(const FBAGroupLink&)> MatchesDirection(EEdGraphPinDirection Dir);

	FBAGroupNodePtr GetNode() const;
	FBAGroupNodePtr GetFromNode() const;
	FBAGroupNodePtr GetToNode() const;

	FBAGroupLink Opposite() const { return FBAGroupLink(To, From); }

	FString ToString() const;

	FSlateRect GetBounds() const;

	bool HasBothLinks() const { return From.IsValid() && To.IsValid(); }

	FPinLink AsRegularPinLink();

	FBAPinLink AsBAPinLink();

	bool IsExecLink();
	bool IsParamLink();

	float CalcStraightenDist() const;
	void Straighten();

	bool IsOnSameBranch() const;

	bool IsOnSameRootBranch() const;

	bool operator==(const FBAGroupLink& Other) const
	{
		if (FallbackNode)
		{
			return FallbackNode == Other.FallbackNode && DirectionOverride == Other.DirectionOverride;
		}

		return (From == Other.From && To == Other.To);
	}

	bool operator<=>(const FBAGroupLink& Other) const
	{
		return (Other == *this) || (Other.Opposite() == *this);
	}

	bool operator!=(const FBAGroupLink& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FBAGroupLink& Link)
	{
		if (Link.FallbackNode)
		{
			return HashCombine(GetTypeHash(Link.FallbackNode), GetTypeHash(Link.DirectionOverride));
		}

		return HashCombine(GetTypeHash(Link.From), GetTypeHash(Link.To));
	}
};

// ======================================
// Pin
// ======================================

struct FBAGroupPin : public TSharedFromThis<FBAGroupPin>
{
	explicit FBAGroupPin(FBAGroupNodePtr InNode, FBAPinPtr InPin, int InPinIndex);

	FBAGroupNodePtr Node;

	FBAPinPtr PinHandle;

	TArray<FBAGroupPinPtr> LinkedTo;

	TArray<FBAGroupLink> GetLinks();

	EEdGraphPinDirection Direction;

	int PinIndex;

	// FSlateRect GetBounds();
 	FBAGroupPinPtr GetPreviousPin();
	FBAGroupPinPtr GetNextPin();

	FVector2D GetPos();

	bool IsExecPin();
	bool IsParameterPin();

	FBAGroupNodePtr GetOwningNode() { return Node; }

	FString GetName();

	float CachedPinOffset = 0.0f;

	static bool HasLinks(FBAGroupPinPtr Pin) { return Pin->LinkedTo.Num() > 0;}
};

// ======================================
// Node
// ======================================

struct FBAGroupNode : public TSharedFromThis<FBAGroupNode>
{
	virtual ~FBAGroupNode() = default;
	explicit FBAGroupNode() = default;
	explicit FBAGroupNode(
		FBAGroupGraphPtr Graph,
		const TArray<FBANodePtr>& Nodes,
		const FSlateRect& InBounds,
		const FString& InNodeName);

	void InitPins();

	FBAGroupGraphPtr Graph;
	TArray<FBANodePtr> Nodes;
	TArray<FBAGroupPinPtr> Pins;

	/* Parent / Child node  */
	FBAGroupLink ParentLink;
	TArray<FBAGroupLink> Children;
	void SetParentLink(const FBAGroupLink& NewParentLink);
	FBAGroupNodePtr GetParentNode() const { return ParentLink.GetFromNode(); }
	virtual FBATreeBranchPtr GetBranch() { return Branch; }
	virtual FBATreeBranchPtr GetRootBranch();
	virtual TArray<FBAGroupNodePtr> GetChildNodes(EEdGraphPinDirection Direction = EGPD_MAX, bool bRecursive = true, bool bInitialDirectionOnly = true);

	FBAGroupNodePtr GetPreviousNode();

	/* Sub nodes for comments */
	virtual TArray<FBAGroupNodePtr> GetSubNodes(bool bRecursive) { return {}; }

	TArray<FBANodePtr> GetContainedNodes(bool bIncludeSubNodes = true);
	FBAVector2 GetContainedNodePos(FBANodePtr Node);

	virtual int32 GetX() const { return NodePosX; }
	virtual int32 GetY() const { return NodePosY; }

	virtual void SetX(int32 X);
	virtual void SetY(int32 Y);

	virtual void MoveX(int32 Delta) { SetX(NodePosX + Delta); }
	virtual void MoveY(int32 Delta) { SetY(NodePosY + Delta); }

	FVector2D Size;
	FString NodeName;
	FBAVector2 GetPos() const { return FBAVector2(NodePosX, NodePosY); }

	virtual FSlateRect GetBounds();
	virtual FSlateRect GetSubNodeBounds();

	virtual TArray<FBAGroupPinPtr> GetPins(EEdGraphPinDirection Direction = EGPD_MAX);
	virtual TArray<FBAGroupLink> GetLinks(EEdGraphPinDirection Direction = EGPD_MAX);
	TArray<FBAGroupNodePtr> GetLinkedNodes(EEdGraphPinDirection Direction = EGPD_MAX);

	TMap<FBANodePtr, FBAVector2> NodeOffsets;


	void UpdateNodeObjects();

	TArray<FBATreeBranchPtr> GetSubBranches();
	int GetNodeIndex();

	TArray<FBAPinPtr> GetBAPins(EEdGraphPinDirection Direction = EGPD_MAX);

	void GetAllNodesInDir(TSet<FBAGroupNodePtr>& OutNodes, EEdGraphPinDirection Dir = EGPD_MAX);


protected:
	TSharedPtr<FBATreeBranch> Branch;

	friend struct FBATreeBranch;
	friend struct FBATree;

private:
	int32 NodePosX = 0;
	int32 NodePosY = 0;
};

using FBAGroupNode_CommentPtr = TSharedPtr<struct FBAGroupNode_Comment>;
struct FBAGroupNode_Comment : public FBAGroupNode
{
	FBAGroupNode_Comment(const FBAGroupGraphPtr& InGraph, TSharedPtr<FBACommentContainsNode> InContainsNode);

	TSharedPtr<FBACommentContainsNode> ContainsNode;

	TArray<FBAGroupNodePtr> SubNodes;
	TSet<FBAGroupNode_CommentPtr> SubComments;

	virtual TArray<FBAGroupNodePtr> GetSubNodes(bool bRecursive) override;

	virtual TArray<FBAGroupPinPtr> GetPins(EEdGraphPinDirection Direction = EGPD_MAX) override;

	virtual TArray<FBAGroupLink> GetLinks(EEdGraphPinDirection Direction = EGPD_MAX) override;

	virtual TArray<FBAGroupNodePtr> GetChildNodes(EEdGraphPinDirection Direction = EGPD_MAX, bool bRecursive = true, bool bInitialDirectionOnly = true) override;

	virtual FSlateRect GetBounds() override;
	virtual FSlateRect GetSubNodeBounds() override;

	virtual FBATreeBranchPtr GetBranch() override;
};

// ======================================
// Graph
// ======================================

struct FBAGroupGraph : public TSharedFromThis<FBAGroupGraph>
{
	TArray<FBAGroupNodePtr> Nodes;
	TSet<FBANodePtr> NodeTree;
	FBAGraphPtr FullGraph; 
	TSharedPtr<FBAGraphHandler> GraphHandler;
	TFunction<bool(FBAPin&)> LinkFilter;

	FBAGroupNodePtr FindNode(UEdGraphNode* Node) const { return FindNode(FullGraph->FindNode(Node)); }

	FBAGroupNodePtr FindNode(FBANodePtr Node) const { return NodeMapping.FindRef(Node); }

	void MakeGraph(FBAGraphPtr InFullGraph, TSharedPtr<FBAGraphHandler> InGraphHandler, const TArray<UEdGraphNode*>& EdGraphNodes);

	void MakeGraph(FBAGraphPtr InFullGraph, TSharedPtr<FBAGraphHandler> InGraphHandler, const TSet<FBANodePtr>& NodePtrs);

	void MakeGraph(FBAGraphPtr InFullGraph, TSharedPtr<FBAGraphHandler> InGraphHandler, const TArray<TSet<FBANodePtr>>& NodeGroups);

	TSet<TSharedPtr<FBAGroupNode_Comment>> MakeCommentGraph(TArray<FBAGroupNodePtr>& RootGraph, const TSet<TSharedPtr<FBACommentContainsNode>>& RootContainsComments);

	TArray<FBAGroupNodePtr> MakeCommentSubset(const TArray<FBAGroupNodePtr>& AllNodes, const TSet<TSharedPtr<FBACommentContainsNode>>& ContainsNodes);

	void PrintGraph();

	void ApplyGraph(TOptional<FText> Transaction = {});

	void MakeSnapshot();

protected:
	TMap<FBANodePtr, FBAGroupNodePtr> NodeMapping;

	void InitConnections();
};

inline FString GetNameSafe(FBAGroupPinPtr Pin)
{
	if (Pin)
	{
		return Pin->GetName();
	}
	else
	{
		return TEXT("none");
	}
}

inline FString GetNameSafe(FBAGroupNodePtr Node)
{
	if (Node)
	{
		return Node->NodeName;
	}
	else
	{
		return TEXT("none");
	}
}