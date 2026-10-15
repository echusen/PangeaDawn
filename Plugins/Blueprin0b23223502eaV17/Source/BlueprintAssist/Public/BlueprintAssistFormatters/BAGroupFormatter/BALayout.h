// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BATree.h"
#include "BAGroupGraph.h"
#include "BlueprintAssistFormatters/BlueprintAssistCommentHandler.h"
#include "BlueprintAssistFormatters/FormatterInterface.h"
#include "EdGraph/EdGraphNode.h"

struct FBATreeGraphContext;
struct FKnotNodeCreation;
class FEdGraphParameterFormatter;
class FBAGraphHandler;
struct FKnotNodeTrack;
struct FNodeInfo;
class FSlateRect;
class UEdGraphNode_Comment;
struct FBACommentContainsNode;

enum class EBALayoutType
{
	Exec,
	Params
};

struct FBALayoutConfig
{
	FIntPoint NodePadding;
	int VerticalPinSpacing;
	bool bCenterBranches;
	bool bForceBranchFromRoot = false;
	EBALayoutType LayoutType = EBALayoutType::Exec;
};

class FBALayout
{
public:
	TSet<FBANodePtr> NodePool;
	TSet<FBANodePtr> RequiredNodes;
	TSet<FBANodePtr> IgnoredNodes;

	FBATree Tree;
	TSharedPtr<FBAGraph> MasterGraph;
	TSharedPtr<FBAGroupGraph> GroupGraph;

	FBAGroupNodePtr GroupRootNode;
	FBANodePtr MasterRootNode;
	TSet<FBAGroupNode_CommentPtr> RootComments;

	bool bApplyHelixing = false;

	FBALayout(
		TSharedPtr<FFormatterInterface> InFormatter,
		TSharedPtr<FBAGraphHandler> InGraphHandler,
		FBAGraphPtr InMasterGraph,
		FBANodePtr InMasterRootNode,
		EEdGraphPinDirection InGraphDirection,
		const FBALayoutConfig& Config, 
		TFunction<bool(const FBAPinLink&)> InLinkFilter,
		TFunction<bool(FBAPin&)> InPinFilter
	);

	virtual ~FBALayout() = default;

	void InitGroupGraph();
	void InitGroupGraph(const TArray<TSet<FBANodePtr>>& NodeGroups);

	virtual void FormatNode(UEdGraphNode* Node);
	virtual void FormatX();

	virtual void FormatY();
	void AvoidPreviouslyLinkedPin();
	void ApplyHelixing();

	virtual void PostFormatting();

	void InitComments();
	void ApplyCommentX();
	void ApplyCommentY();

	void StraightenTree();
	void ExpandNodesAheadOfParameters();
	void MoveParamsBelowExecLine();

	virtual TSet<UEdGraphNode*> GetFormattedNodes();

	virtual FCommentHandler* GetCommentHandler() { return Formatter->GetCommentHandler(); }

	TArray<UEdGraphNode*> GetNodePoolAsObjs() const;

private:
	EBALayoutType LayoutType;
	EEdGraphPinDirection GraphDirection;

	FIntPoint NodePadding;
	int VerticalPinSpacing;
	bool bCenterBranches;

	TSharedPtr<FFormatterInterface> Formatter;
	TSharedPtr<FBAGraphHandler> GraphHandler;

	TFunction<bool(const FBAPinLink&)> LinkFilter;
	TFunction<bool(FBAPin&)> PinFilter;

	void InitNodePool(FBANodePtr InitialNode, TSet<FBANodePtr>& OutNodePool);

	/** X formatting */
	void LayoutFormatXTree();
	void DecideXParents(TArray<FBAGroupLink> InitialLinks);
	int32 GetChildX(const FBAGroupLink& Link);

	/** Y formatting */
	void LayoutFormatYTree();

	/** Comment */
	void ApplyCommentX_Recursive(TArray<FBAGroupNodePtr> NodeSet, const TSet<FBAGroupNode_CommentPtr>& Comments);
	void ApplyCommentY_Recursive(TArray<FBAGroupNodePtr> NodeSet, const TSet<FBAGroupNode_CommentPtr>& Comments);

	FBAGroupNode_CommentPtr FindParentComment(const FBAGroupNodePtr& Node, const TSet<FBAGroupNode_CommentPtr>& Comments);

	bool IsValidLink(const FBAPinLink& Link);
};
