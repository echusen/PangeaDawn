// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraphSnapshotManager.h"
#include "BATree.h"
#include "BAKnotTrackManager.h"
#include "BALayout.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistFormatters/BlueprintAssistCommentHandler.h"
#include "BlueprintAssistFormatters/FormatterInterface.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"
#include "EdGraph/EdGraphNode.h"

struct FKnotNodeCreation;
class FEdGraphParameterFormatter;
class FBAGraphHandler;
struct FKnotNodeTrack;
struct FNodeInfo;
class FSlateRect;
class UEdGraphNode_Comment;
struct FBACommentContainsNode;
class FBALayout;

class FBAFormatter_Tree final
	: public FFormatterInterface
{
public:
	TSharedPtr<FBAGraphHandler> GraphHandler;

	FBAFormatter_Tree(
		TSharedPtr<FBAGraphHandler> InGraphHandler,
		FEdGraphFormatterParameters InFormatterParameters);

	virtual ~FBAFormatter_Tree() override { }

	virtual void FormatNode(UEdGraphNode* Node) override;

	virtual TSet<UEdGraphNode*> GetFormattedNodes() override;

	virtual FCommentHandler* GetCommentHandler() override { return &CommentHandler; }

	virtual UEdGraphNode* GetRootNode() override;

	virtual bool ShouldIgnoreComment(TSharedPtr<FBACommentContainsNode> ContainsNode) override;

	bool ShouldFormatLink(const FPinLink& Link);

	virtual FEdGraphFormatterParameters& GetFormatterParameters() override { return FormatterParameters; }
	virtual FBAFormatterSettings GetFormatterSettings() override;

	TSharedPtr<FBALayout> FinalLayout;

private:
	UEdGraphNode* RootNodeObj;
	FIntPoint InitialLocation;

	TSharedPtr<FBAGraph> MasterGraph; 
	FBANodePtr MasterRootNode;

	FCommentHandler CommentHandler;
	FBAKnotTrackManager KnotTrackManager;

	FEdGraphFormatterParameters FormatterParameters;
	FBAFormatterSettings FormatterSettings;
	TSharedPtr<FBAScopedSnapshotGroup> MainSnapshotGroup;

	void FormatParameters(TArray<TSet<FBANodePtr>>& ParamGroups);

	virtual void PostFormatting() override;

	TSharedRef<FBALayout> MakeMainLayout(FBANodePtr Node);
	TSharedRef<FBALayout> MakeParamLayout(FBANodePtr Node);

	void RemoveKnotNodes();

	void ResetInitialLocation();
};
