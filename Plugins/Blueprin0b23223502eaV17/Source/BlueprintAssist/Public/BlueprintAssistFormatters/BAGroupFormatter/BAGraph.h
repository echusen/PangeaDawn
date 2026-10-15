// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistTypes.h"

class UBAGraphSchema;
class UEdGraphNode;
struct FBAPin;
struct FBAGraph;
struct FBANode;

using FBAPinPtr = TSharedPtr<FBAPin>;
using FBANodePtr = TSharedPtr<FBANode>;
using FBAGraphPtr = TSharedPtr<FBAGraph>;

// ======================================
// PinLink
// ======================================

struct FBAPinLink
{
	FBAPinPtr From;
	FBAPinPtr To;

	// sometimes we use a fallback node and a direction to kickstart iteration
	FBANodePtr FallbackNode;
	EEdGraphPinDirection DirectionOverride = EGPD_MAX;

	FBAPinLink() {}
	FBAPinLink(const FBAPinPtr& InFrom, const FBAPinPtr& InTo) : From(InFrom), To(InTo) {}
	FBAPinLink(const FBANodePtr& InNode, const EEdGraphPinDirection& InDirection) : FallbackNode(InNode), DirectionOverride(InDirection) {}

	EEdGraphPinDirection GetDirection() const;

	FBANodePtr GetNode() const;
	FBANodePtr GetFromNode() const;
	FBANodePtr GetToNode() const;

	FBAPinLink MakeOppositeLink() const { return FBAPinLink(To, From); }

	FString ToString() const;

	FSlateRect GetBounds() const;

	static bool IsExec(const FBAPinLink& Link);
	static bool IsParameter(const FBAPinLink& Link) { return !IsExec(Link); }

	bool operator==(const FBAPinLink& Other) const
	{
		return From == Other.From && To == Other.To;
	}

	bool operator!=(const FBAPinLink& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FBAPinLink& Link)
	{
		return HashCombine(GetTypeHash(Link.From), GetTypeHash(Link.To));
	}
};

// ======================================
// Pin
// ======================================

struct FBAPin : public TSharedFromThis<FBAPin>
{
	explicit FBAPin(FBANodePtr InNode, UEdGraphPin* InPin);

	FBANodePtr Node;

	FBANodePinHandle PinHandle;

	TArray<FBAPinPtr> LinkedTo;

	TArray<FBAPinLink> GetLinks();

	EEdGraphPinDirection Direction;

	int PinIndex = -1;

	FBAPinPtr GetPreviousPin();
	FBAPinPtr GetNextPin();

	FBAPinPtr GetPreviousLinkedPin();

	FString PinName;

	// FSlateRect GetBounds();

	FVector2D GetPos();

	UEdGraphPin* GetObj();

	bool IsExecPin();
	bool IsParameterPin() { return !IsExecPin(); }

	FBANodePtr GetOwningNode() { return Node; }

	FString GetName(bool bIncludeNodeName = false);

	bool operator==(const FBAPin& Other) const
	{
		return PinHandle == Other.PinHandle;
	}

	bool operator!=(const FBAPin& Other) const
	{
		return !(*this == Other);
	}

	friend inline uint32 GetTypeHash(const FBAPin& Pin)
	{
		return GetTypeHash(Pin.PinHandle);
	}

	float CachedPinOffset = 0.0f;

	bool HasLinks() { return LinkedTo.Num() > 0;}
};

// ======================================
// Node
// ======================================

struct FBANode : public TSharedFromThis<FBANode>
{
	explicit FBANode(FBAGraphPtr Graph, UEdGraphNode* Node);

	void InitPins();

	FBAGraphPtr Graph;
	UEdGraphNode* NodeObj;

	TArray<FBAPinPtr> Pins;

	int32 NodePosX = 0;
	int32 NodePosY = 0;

	bool IsExecNode();
	bool IsParameterNode() { return !IsExecNode(); }

	FSlateRect GetBounds();
	FString GetName();
	FBAVector2 GetPos() const { return FBAVector2(NodePosX, NodePosY); }

	TArray<FBAPinPtr> GetPins(EEdGraphPinDirection Direction = EGPD_MAX);
	TArray<FBAPinLink> GetLinks(EEdGraphPinDirection Direction = EGPD_MAX);

	bool operator==(const FBANode& Other) const
	{
		return Other.NodeObj == NodeObj;
	}

	bool operator!=(const FBANode& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FBANode& Node)
	{
		return GetTypeHash(Node.NodeObj);
	}
};

// ======================================
// Graph
// ======================================

struct FBAGraph : public TSharedFromThis<FBAGraph>
{
	TArray<FBANodePtr> Nodes;
	TSet<UEdGraphNode*> NodeTree;
	TSharedPtr<FBAGraphHandler> GraphHandler;

	FBANodePtr FindNode(UEdGraphNode* Node) const { return NodeMapping.FindRef(Node); }

	static TSharedRef<FBAGraph> MakeGraph(TSharedPtr<FBAGraphHandler> InGraphHandler, UEdGraphNode* RootNode);

	void MakeGraph(TSharedPtr<FBAGraphHandler> InGraphHandler, const TArray<UEdGraphNode*>& EdGraphNodes);

	void PrintGraph();

	void ApplyGraph(TOptional<FText> Transaction = {});

	const UBAGraphSchema& GetSchema() const;

	TArray<UEdGraphNode*> GetNodeObjs() const;

protected:
	TMap<UEdGraphNode*, FBANodePtr> NodeMapping;

	void InitConnections();
};

inline FString GetNameSafe(FBAPinPtr Pin, bool bIncludeNodeName = false)
{
	if (Pin)
	{
		return Pin->GetName(bIncludeNodeName);
	}
	else
	{
		return TEXT("none");
	}
}

inline FString GetNameSafe(FBANodePtr Node)
{
	if (Node)
	{
		return Node->GetName();
	}
	else
	{
		return TEXT("none");
	}
}