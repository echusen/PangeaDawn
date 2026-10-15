// Copyright fpwong. All Rights Reserved.

#include "BAFormatter_Tree.h"

#include "BALayout.h"
#include "BAGraphSnapshotManager.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistUtils.h"
#include "SGraphNodeComment.h"
#include "SGraphPanel.h"
#include "BlueprintAssistFormatters/GraphFormatterTypes.h"
#include "BlueprintAssistMisc/AlgoBA.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor/BlueprintGraph/Classes/K2Node_Knot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Stats/StatsMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogFormatParams, NoLogging, All);

static TAutoConsoleVariable<bool> CVarDisableResetLocation(TEXT("BlueprintAssist.Format.DisableResetLocation"), false, TEXT(""), ECVF_SaveForNextBoot);

FBAFormatter_Tree::FBAFormatter_Tree(
	TSharedPtr<FBAGraphHandler> InGraphHandler,
	FEdGraphFormatterParameters InFormatterParameters)
	: GraphHandler(InGraphHandler)
	, RootNodeObj(nullptr)
	, KnotTrackManager()
	, FormatterParameters(InFormatterParameters)
{
	FormatterSettings = UBASettings::GetFormatterSettings(GraphHandler->GetFocusedEdGraph());
}

void FBAFormatter_Tree::FormatNode(UEdGraphNode* InitialNode)
{
	InitialNode->GetGraph()->Modify();

	RootNodeObj = InitialNode;

	CommentHandler.Init(GraphHandler, AsShared());
	KnotTrackManager.Init(SharedThis<FBAFormatter_Tree>(this));
	RemoveKnotNodes();

	MasterGraph = FBAGraph::MakeGraph(GraphHandler, InitialNode);
	MasterRootNode = MasterGraph->FindNode(InitialNode);

	FBAGraphSnapshotManager::Get().ClearSnapshots();

	InitialLocation.X = MasterRootNode->NodePosX;
	InitialLocation.Y = MasterRootNode->NodePosY;

	// randomize node pos to visualize formatting
	if (BA_DEBUG("Randomize"))
	{
		for (FBANodePtr Node : MasterGraph->Nodes)
		{
			Node->NodePosX += FMath::RandRange(-500, 500);
			Node->NodePosY += FMath::RandRange(-500, 500);
		}
	}

	TArray<FBANodePtr> InitialExecNodes = MasterGraph->Nodes.FilterByPredicate(&FBANode::IsExecNode);
	if (InitialExecNodes.Num() <= 1) // quick format lone tree
	{
		FinalLayout = MakeParamLayout(MasterRootNode);
		FinalLayout->InitGroupGraph();
		FBAScopedSnapshotGroup Snapshot("ParamLayout", FinalLayout->GroupGraph);

		BA_DEBUG_EARLY_EXIT("BuildTree");

		FinalLayout->FormatX();
		FinalLayout->FormatY();
	}
	else
	{
		// format the x position once to get the initial positions for the parameters
		{
			FinalLayout = MakeMainLayout(MasterRootNode);
			FinalLayout->InitGroupGraph();

			BA_DEBUG_EARLY_EXIT("BuildTree");

			MainSnapshotGroup = MakeShared<FBAScopedSnapshotGroup>("Format", FinalLayout->GroupGraph);
			
			{
				FBAScopedSnapshotGroup G1("X1", FinalLayout->GroupGraph);
				FinalLayout->FormatX();
			}

			BA_DEBUG_EARLY_EXIT("X1");
		}

		// format the parameters for each exec node
		TArray<TSet<FBANodePtr>> ParamGroups;
		FormatParameters(ParamGroups);
		BA_DEBUG_EARLY_EXIT("PostParams");

		// Make a new group graph out of the param formatter groups and format fully
		{
			for (auto& Group : ParamGroups)
			{
				if (Group.IsEmpty())
				{
					UE_LOG(LogFormatParams, Error, TEXT("Group empty"));
					return;
				}
			}

			if (!ParamGroups.IsEmpty())
			{
				FinalLayout = MakeMainLayout(MasterRootNode);
				FinalLayout->InitGroupGraph(ParamGroups);

				FBAScopedSnapshotGroup Snapshot("FullFormat", FinalLayout->GroupGraph);
				FinalLayout->FormatX();
				FinalLayout->ExpandNodesAheadOfParameters();

				FinalLayout->StraightenTree();

				FinalLayout->InitComments();
				FinalLayout->ApplyCommentX();

				FinalLayout->FormatY();
				FinalLayout->ApplyCommentY();

				for (FBAGroupNodePtr GroupNode : FinalLayout->GroupGraph->Nodes)
				{
					if (UBASettings_Advanced::HasDebugSetting("DrawGroups"))
					{
						GraphHandler->GetGraphOverlay()->DrawBounds(GroupNode->GetBounds(), FColor::Green);
					}
				}
			}
		}
	}

	if (FormatterSettings.bCreateKnotNodes)
	{
		KnotTrackManager.FormatKnotNodes();
	}

	if (MainSnapshotGroup)
	{
		FBAScopedSnapshotGroup::MakeLeafSnapshot("EndFormatting", FinalLayout->GroupGraph);
	}
}

void FBAFormatter_Tree::FormatParameters(TArray<TSet<FBANodePtr>>& ParamGroups)
{
	// format params for all the exec nodes in the group graph
	TArray<FBAGroupNodePtr> GroupNodes = FinalLayout->GroupGraph->Nodes;
	GroupNodes.StableSort([](const FBAGroupNodePtr& A, const FBAGroupNodePtr& B)
	{
		if (A->GetPos().X != B->GetPos().X)
		{
			return A->GetPos().X < B->GetPos().X;
		}

		return A->GetBranch()->BranchIndex < B->GetBranch()->BranchIndex;
	});

	TArray<FBANodePtr> ExecNodes;
	AlgoBA::TransformAccumulate(GroupNodes, ExecNodes, &FBAGroupNode::Nodes, TAppend());

	TSet<FBANodePtr> IgnoredNodes;
	TMap<FBANodePtr, TSharedPtr<FBALayout>> NodeOwnerMap;
	TArray<TSharedPtr<FBALayout>> ParamFormatters;

	{
		FBAScopedSnapshotGroup G("Params", FinalLayout->GroupGraph);
		for (FBANodePtr Node : ExecNodes)
		{
			TSharedPtr<FBALayout> ParamLayout = MakeParamLayout(Node);
			ParamFormatters.Add(ParamLayout);

			ParamLayout->IgnoredNodes = IgnoredNodes;
			ParamLayout->InitGroupGraph();
			FBAScopedSnapshotGroup ParamGroup(GetNameSafe(Node), ParamLayout->GroupGraph);

			ParamLayout->StraightenTree();

			ParamLayout->FormatX();
			ParamLayout->FormatY();
			ParamLayout->MoveParamsBelowExecLine();

			if (ParamLayout->Tree.RootBranch)
			{
				UE_LOG(LogFormatParams, VeryVerbose, TEXT("Owned parameter node for %s"), *Node->GetName());

				// These input nodes are the nodes we 'own' - tell later formatting to ignore this
				TSet<FBAGroupNodePtr> OwnedInputParams;
				ParamLayout->GroupRootNode->GetAllNodesInDir(OwnedInputParams, EGPD_Input);

				for (FBAGroupNodePtr OwnedGroupNodes : OwnedInputParams)
				{
					IgnoredNodes.Append(OwnedGroupNodes->Nodes);

					for (auto N : OwnedGroupNodes->Nodes)
					{
						UE_LOG(LogFormatParams, VeryVerbose, TEXT("\t%s"), *N->GetName());
					}
				}

				// Own all nodes in our node pool (these nodes can be "stolen" later)
				for (FBANodePtr OwnedNode : ParamLayout->NodePool)
				{
					// Take nodes from previous owners
					if (TSharedPtr<FBALayout> PrevOwner = NodeOwnerMap.FindRef(OwnedNode))
					{
						if (PrevOwner != ParamLayout)
						{
							UE_LOG(LogFormatParams, Warning, TEXT("Param owner CHANGED for %s (prev %s)"), *GetNameSafe(OwnedNode), *GetNameSafe(PrevOwner->MasterRootNode));
							PrevOwner->NodePool.Remove(OwnedNode);
						}
					}

					UE_LOG(LogFormatParams, Warning, TEXT("Setting owner for %s to %s"), *GetNameSafe(OwnedNode), *GetNameSafe(Node));
					NodeOwnerMap.Add(OwnedNode, ParamLayout);
				}
			}
		}
	}

	FBADrawBuilder Draw;
	for (TSharedPtr<FBALayout> Layout : ParamFormatters)
	{
		// debug draw
		{
			FLinearColor GroupColor = FLinearColor::MakeRandomColor();
			for (FBANodePtr Node : Layout->NodePool)
			{
				Draw.Add(MakeShared<FBADrawNodeAction>(Node->NodeObj, GraphHandler->GetGraphPanel(), GroupColor));
			}
		}

		if (Layout->NodePool.IsEmpty())
		{
			UE_LOG(LogFormatParams, Error, TEXT("Error empty %s"), *GetNameSafe(Layout->MasterRootNode));
		}

		ParamGroups.Add(Layout->NodePool);
		UE_LOG(LogFormatParams, VeryVerbose, TEXT("BEGIN LOG PARAM NODE POOL %s"), *GetNameSafe(Layout->MasterRootNode));
		for (auto N : Layout->NodePool)
		{
			UE_LOG(LogFormatParams, VeryVerbose, TEXT("\t%s"), *GetNameSafe(N));
		}
		UE_LOG(LogFormatParams, VeryVerbose, TEXT("END LOG PARAM NODE POOL %s"), *GetNameSafe(Layout->MasterRootNode));
	}

	if (BA_DEBUG("DrawParamGroups"))
	{
		GraphHandler->GetGraphOverlay()->AddDrawAction(Draw.Timed(3.0f));
	}
}

void FBAFormatter_Tree::PostFormatting()
{
#if 1
	MainSnapshotGroup.Reset();

	if (FinalLayout)
	{
		if (!CVarDisableResetLocation->GetBool())
		{
			ResetInitialLocation();
		}

		FinalLayout->PostFormatting();
	}
#endif
}

TSharedRef<FBALayout> FBAFormatter_Tree::MakeMainLayout(FBANodePtr Node)
{
	const UBASettings& BASettings = UBASettings::Get();

	FBALayoutConfig Config;
	Config.LayoutType = EBALayoutType::Exec;
	Config.NodePadding = BASettings.BlueprintFormatterSettings.Padding;
	Config.VerticalPinSpacing = BASettings.ParameterVerticalPinSpacing; // unused
	Config.bCenterBranches = BASettings.bCenterBranches;

	const auto LinkFilter = [](const FBAPinLink& Link)
	{
		return FBAPinLink::IsExec(Link);
	};

	return MakeShared<FBALayout>(AsShared(), GraphHandler, MasterGraph, Node, FormatterSettings.FormatterDirection, Config, LinkFilter, &FBAPin::IsExecPin);
}

TSharedRef<FBALayout> FBAFormatter_Tree::MakeParamLayout(FBANodePtr Node)
{
	const UBASettings& BASettings = UBASettings::Get();

	FBALayoutConfig Config;
	Config.LayoutType = EBALayoutType::Params;
	Config.NodePadding = BASettings.BlueprintParameterPadding;
	Config.VerticalPinSpacing = BASettings.ParameterVerticalPinSpacing;
	Config.bCenterBranches = BASettings.bCenterBranchesForParameters;
	Config.bForceBranchFromRoot = true;

	const auto LinkFilter = [](const FBAPinLink& Link)
	{
		return Link.GetToNode()->IsParameterNode();
	};

	return MakeShared<FBALayout>(AsShared(), GraphHandler, MasterGraph, Node, EGPD_Input, Config, LinkFilter, &FBAPin::IsParameterPin);
}

void FBAFormatter_Tree::RemoveKnotNodes()
{
	UE_LOG(LogBAComments, Log, TEXT("[%hs]"), __FUNCTION__);
	const auto ExecFilter = [&](const FPinLink& Link)
	{
		return FBAUtils::IsExecOrDelegatePinLink(Link) && ShouldFormatLink(Link);
	};

	const auto PureFilter = [&](const FPinLink& Link)
	{
		return FBAUtils::IsNodePure(Link.GetToNodeUnsafe()) && ShouldFormatLink(Link);
	};

	TSet<UEdGraphNode*> ExecTree = FBAUtils::GetNodeTreeWithFilter(GetRootNode(), ExecFilter);

	TSet<UEdGraphNode*> AllNodes = ExecTree;
	for (UEdGraphNode* Node : ExecTree)
	{
		AllNodes.Append(FBAUtils::GetNodeTreeWithFilter(Node, PureFilter, EGPD_MAX, true));
	}

	KnotTrackManager.RemoveKnotNodes(AllNodes.Array());
}

void FBAFormatter_Tree::ResetInitialLocation()
{
	const float DeltaX = InitialLocation.X - MasterRootNode->NodePosX;
	const float DeltaY = InitialLocation.Y - MasterRootNode->NodePosY;

	if (DeltaX != 0 || DeltaY != 0)
	{
		for (FBANodePtr Node : MasterGraph->Nodes)
		{
			Node->NodePosX += DeltaX;
			Node->NodePosY += DeltaY;
		}

		for (UEdGraphNode* Node : KnotTrackManager.GetSpawnedKnotNodes())
		{
			Node->NodePosX += DeltaX;
			Node->NodePosY += DeltaY;
		}
	}
}

TSet<UEdGraphNode*> FBAFormatter_Tree::GetFormattedNodes()
{
	TSet<UEdGraphNode*> OutNodes;

	if (FinalLayout && FinalLayout->GroupGraph)
	{
		for (FBAGroupNodePtr GroupNode : FinalLayout->GroupGraph->Nodes)
		{
			Algo::Transform(GroupNode->GetContainedNodes(), OutNodes, &FBANode::NodeObj);
		}
	}

	OutNodes.Append(KnotTrackManager.GetSpawnedKnotNodes());

	return OutNodes;
}

UEdGraphNode* FBAFormatter_Tree::GetRootNode()
{
	return RootNodeObj;
}

bool FBAFormatter_Tree::ShouldIgnoreComment(TSharedPtr<FBACommentContainsNode> ContainsNode)
{
	TSet<UEdGraphNode*> FormattedNodes = GetFormattedNodes();
	const TArray<UEdGraphNode*>& AllNodesUnderComment = ContainsNode->AllContainedNodes;

	// ignore containing comments
	TSet<UEdGraphNode*> NodesUnderComment(AllNodesUnderComment);

	if (NodesUnderComment.Num() == 0)
	{
		// UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("\tIGNORE: Skipping comment EMPTY"));
		return true;
	}

	// ignore if the comment contains a node which isn't going to be formatted 
	for (UEdGraphNode* Node : NodesUnderComment)
	{
		if (!FormattedNodes.Contains(Node))
		{
			UE_LOG(LogBAComments, Log, TEXT("[%hs] Ignore comment %s: Missing node %s"), __FUNCTION__, *FBAUtils::GetNodeName(ContainsNode->Comment), *FBAUtils::GetNodeName(Node));
			return true;
		}

		// TODO
		// ignore if nodes underneath contains parameter but not its parent
		// GetFormatterParameters()
		// if (TSharedPtr<FEdGraphParameterFormatter> ParamFormatter = GetParameterParent(Node))
		// {
		// 	if (!NodesUnderComment.Contains(ParamFormatter->GetRootNode()))
		// 	{
		// 		// UE_LOG(LogTemp, Error, TEXT("\tIGNORE Param form"));
		// 		return true;
		// 	}
		// }
	}

	// ignore if all nodes are not in the same node tree 
	const auto IsUnderComment = [&NodesUnderComment](const FPinLink& PinLink)
	{
		return NodesUnderComment.Contains(PinLink.GetNode());
	};

	const TSet<UEdGraphNode*> CommentNodeTree = FBAUtils::GetNodeTreeWithFilter(AllNodesUnderComment[0], IsUnderComment);
	if (CommentNodeTree.Num() != NodesUnderComment.Num())
	{
		UE_LOG(LogBAComments, Log, TEXT("[%hs] Ignore comment %s: Node Tree Count Doesn't Match"), __FUNCTION__, *FBAUtils::GetNodeName(ContainsNode->Comment));
		return true;
	}

	for (UEdGraphNode* Node : NodesUnderComment)
	{
		if (!CommentNodeTree.Contains(Node) && !FBAUtils::IsKnotNode(Node))
		{
			UE_LOG(LogBAComments, Log, TEXT("[%hs] Ignore comment %s: Not in Node Tree (%s)"), __FUNCTION__, *FBAUtils::GetNodeName(ContainsNode->Comment), *FBAUtils::GetNodeName(Node));
			return true;
		}
	}

	return false;
}

bool FBAFormatter_Tree::ShouldFormatLink(const FPinLink& Link)
{
	// TODO use ignored / required nodes
	return true;
}

FBAFormatterSettings FBAFormatter_Tree::GetFormatterSettings()
{
	return FormatterSettings;
}
