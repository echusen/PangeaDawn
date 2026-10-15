// Copyright fpwong. All Rights Reserved.

#include "BAKnotTrackManager.h"

#include "BAFormatter_Tree.h"
#include "BALayout.h"
#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistStats.h"
#include "BlueprintAssistUtils.h"
#include "K2Node_Knot.h"
#include "BlueprintAssistFormatters/BlueprintAssistCommentHandler.h"
#include "BlueprintAssistFormatters/FormatterInterface.h"
#include "BlueprintAssistMisc/AlgoBA.h"
#include "BlueprintAssistMisc/BAGraphSchema.h"
#include "BlueprintAssistWidgets/BlueprintAssistGraphOverlay.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Stats/StatsMisc.h"

#define DEBUG_DRAW_TRACKS 0

void FBAKnotTrackManager::Init(TSharedPtr<FBAFormatter_Tree> InFormatter)
{
	Formatter = InFormatter;
	GraphHandler = Formatter->GraphHandler;

	const UBASettings& BASettings = UBASettings::Get();
	NodePadding = BASettings.BlueprintFormatterSettings.Padding;
	PinPadding = BASettings.BlueprintParameterPadding;
	TrackSpacing = BASettings.BlueprintKnotTrackSpacing;
}

void FBAKnotTrackManager::SetTrackHeight(FBAKnotTrackPtr Track)
{
	// const auto& AllNodes = Formatter->MasterGraph->Nodes;
	UE_LOG(LogKnotTrack, Log, TEXT("[%hs] Set Track Height %s"), __FUNCTION__, *Track->ToString());//, *Track->Branch->ToString())

	FSlateRect TrackBounds = Track->GetTrackBounds().ExtendBy(FMargin(-10, 0));
	auto Root = Track->GetRootPin(); 
	auto Last = Track->GetLastPin();

	bool bIsParam = Track->GetRootPin()->IsParameterPin();

	// UE_LOG(LogKnotTrack, Error, TEXT("FindTrackHeight for pin (%s) | Start %f | End %f"), *FBAUtils::GetPinName(Root), TrackStart, TrackEnd);

	if (Track->bIsLoopingTrack)
	{
		FBANodePtr LowestNode = Root->GetOwningNode()->NodePosY < Last->GetOwningNode()->NodePosY ? Root->GetOwningNode() : Last->GetOwningNode();
		Track->UpdateTrackHeight(LowestNode->NodePosY - 1);
		TrackRelatedNodes.FindOrAdd(LowestNode).Add(Track);
		return;
	}

	// set starting point
	float TestSolution = Last->GetPos().Y;

	// collide with our expanded node set in the branch
	TSet<FBANodePtr> AllNodes;
	AlgoBA::TransformAccumulate(Track->FirstBranch->GetExpandedNodes(), AllNodes, &FBAGroupNode::Nodes, TAppend());
	AlgoBA::TransformAccumulate(Track->LastBranch->GetExpandedNodes(), AllNodes, &FBAGroupNode::Nodes, TAppend());

	FVector2D StartPoint(TrackBounds.Left, TestSolution);
	FVector2D EndPoint(TrackBounds.Right, TestSolution);

	FBANodePtr LastCollidingNode;
	for (int i = 0; i < 100; ++i)
	{
		bool bNoCollisionInDirection = true;

		// if (bIsParam)
		{
			for (auto NodeToCollisionCheck : AllNodes)
			{
				const bool bSkipNode = NodeToCollisionCheck == Root->GetOwningNode() || NodeToCollisionCheck == Last->GetOwningNode();
				if (!bSkipNode)
				{
					FSlateRect NodeBounds = NodeToCollisionCheck->GetBounds().ExtendBy(FMargin(0, 0, 0, TrackSpacing));

					UE_LOG(LogKnotTrack, Log, TEXT("\t%hs Collision checking %s"), __FUNCTION__, *GetNameSafe(NodeToCollisionCheck))
					if (FBAUtils::LineRectIntersection(NodeBounds, StartPoint, EndPoint))
					{
						UE_LOG(LogKnotTrack, Error, TEXT("\tNode collision  (%s) (%f) | %s"), *GetNameSafe(NodeToCollisionCheck), TestSolution, *NodeBounds.ToString());
						bNoCollisionInDirection = false;
						LastCollidingNode = NodeToCollisionCheck;
						StartPoint.Y = EndPoint.Y = TestSolution = NodeBounds.Bottom + 1;
						#if DEBUG_DRAW_TRACKS
											GraphHandler->GetGraphOverlay()->DrawBounds(NodeBounds, FLinearColor::Yellow);
											GraphHandler->GetGraphOverlay()->DrawBounds(TrackBounds, FLinearColor::Red);
						#endif
					}
				}
			}
		}

		if (bNoCollisionInDirection)
		{
			if (LastCollidingNode)
			{
				TrackRelatedNodes.FindOrAdd(LastCollidingNode).Add(Track);
			}

			Track->UpdateTrackHeight(TestSolution);
			break;
		}
	}
}

void FBAKnotTrackManager::FormatKnotNodes()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBAKnotTrackManager::FormatKnotNodes"), STAT_BAKnotTrackManager_FormatNode, STATGROUP_BA_EdGraphFormatter);
	//UE_LOG(LogKnotTrack, Log, TEXT("### Format Knot Nodes"));

	MakeKnotTrack();


	MakeTrackGroups(KnotTracks);

	SpawnKnotNodes();

	for (auto TrackGroup : TrackGroups)
	{
		// UE_LOG(LogKnotTrack, Log, TEXT("LIST TRACK GROUPS"));
		if (UBASettings_Advanced::HasDebugSetting("DebugTracks"))
		{
			GraphHandler->GetGraphOverlay()->DrawBounds(TrackGroup->GetBounds().ExtendBy(FMargin(4.0f)), FLinearColor::Red);
		}

		// for (auto Track : TrackGroup->Tracks)
		// {
		// 	// for (const auto & Creation : Track->KnotCreations)
		// 	// {
		// 	// 	if (Creation->CreatedKnot)
		// 	// 	{
		// 	// 		KnotTrackGroup.Add(Creation->CreatedKnot, TrackGroup);
		// 	// 		KnotCreationMap.Add(Creation->CreatedKnot, Creation);
		// 	// 	}
		// 	// }
		//
		// 	// UE_LOG(LogKnotTrack, Log, TEXT("\t%s"), *Track->ToString());
		// 	if (UBASettings_Advanced::HasDebugSetting("DebugTracks"))
		// 	{
		// 		GraphHandler->GetGraphOverlay()->DrawBounds(Track->GetTrackBounds(), Track->HasPinToAlignTo() ? FLinearColor::Yellow : FLinearColor::White);
		// 	}
		// }
	}

	if (!BA_DEBUG("AddKnotsToComments"))
	{
		AddKnotNodesToComments();
		AddNomadKnotsIntoComments();
	}

	for (auto Track : KnotTracks)
	{
		if (UBASettings_Advanced::HasDebugSetting("DebugTracks"))
		{
			GraphHandler->GetGraphOverlay()->DrawBounds(Track->GetTrackBounds(), Track->HasPinToAlignTo() ? FLinearColor::Yellow : FLinearColor::White);
		}
	}

	UE_LOG(LogKnotTrack, Warning, TEXT("BEGIN LOG TRACK RELATED NODE"));
	for (auto& [Node, Tracks] : TrackRelatedNodes)
	{
		UE_LOG(LogKnotTrack, Log, TEXT("Node %s"), *Node->GetName());
		for (auto Track : Tracks)
		{
			UE_LOG(LogKnotTrack, Log, TEXT("\tTrack %s"), *Track->ToString());
		}
	}
	UE_LOG(LogKnotTrack, Warning, TEXT("END LOG TRACK RELATED NODE"));
}

void FBAKnotTrackManager::RemoveKnotNodes(const TArray<UEdGraphNode*>& NodeTree)
{
	TArray<UEdGraphNode_Comment*> CommentNodes = FBAUtils::GetCommentNodesFromGraph(GraphHandler->GetFocusedEdGraph());

	for (auto Node : NodeTree)
	{
		/** Delete all connections for each knot node */
		if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(Node))
		{
			FBAUtils::DisconnectKnotNode(KnotNode);

			for (auto Comment : CommentNodes)
			{
				if (Comment->GetNodesUnderComment().Contains(KnotNode))
				{
					FBAUtils::RemoveNodeFromComment(Comment, KnotNode);
				}
			}

			// if (UBASettings::Get().bUseKnotNodePool &&
			// 	UBASettings::Get().bCreateKnotNodes) // if we don't create knot nodes, no point reusing them
			// {
			// 	KnotNodePool.Add(KnotNode);
			// }
			// else
			{
				FBAUtils::DeleteNode(KnotNode);

				if (FCommentHandler* CH = Formatter->GetCommentHandler())
				{
					CH->DeleteNode(KnotNode);
				}
			}
		}
	}
}

bool FBAKnotTrackManager::TryAlignTrackToEndPins(FBAKnotTrackPtr Track, const TArray<FBANodePtr>& AllNodes)
{
	const float ParentPinY = Track->GetRootPin()->GetPos().Y;
	const float LastPinY = Track->GetLastPin()->GetPos().Y;
	bool bPreferParentPin = ParentPinY > LastPinY;

	if (Track->GetRootPin()->IsExecPin())
	{
		bPreferParentPin = false;
	}

	for (int i = 0; i < 2; ++i)
	{
		if (i == 1)
		{
			bPreferParentPin = !bPreferParentPin;
		}

		// FString PreferPinStr = bPreferParentPin ? "true" : "false";
		// UE_LOG(LogKnotTrack, Log, TEXT("AlignTrack %s ParentY %f (%s) | LastPinY %f (%s) | PreferParent %s"), *Track->ToString(), ParentPinY, *FBAUtils::GetPinName(Track->GetRootPin()), LastPinY,
		//        *FBAUtils::GetPinName(Track->GetLastPin()), *PreferPinStr);

		FBAPinPtr SourcePin = bPreferParentPin ? Track->GetRootPin() : Track->GetLastPin();
		FBAPinPtr OtherPin = bPreferParentPin ? Track->GetLastPin() : Track->GetRootPin();

		const FVector2D SourcePinPos =  SourcePin->GetPos();
		const FVector2D OtherPinPos =  OtherPin->GetPos();

		// don't align if the last pin is lower than the parent pin 
		if (!bPreferParentPin && LastPinY < ParentPinY)
		{
			continue;
		}

		const FIntPoint Padding = OtherPin->IsParameterPin()
			? PinPadding
			: NodePadding;

		const FVector2D Point
			= SourcePin->Direction == EGPD_Output
			? FVector2D(OtherPinPos.X - Padding.X, SourcePinPos.Y)
			: FVector2D(OtherPinPos.X + Padding.X, SourcePinPos.Y);

		// GraphHandler->GetGraphOverlay()->DrawLine(SourcePinPos, Point);

		// UE_LOG(LogTemp, Error, TEXT("Checking Point %s | %s"), *Point.ToString(), *FBAUtils::GetNodeName(SourcePin->GetOwningNode()));

		bool bAnyCollision = false;

		for (FBANodePtr NodeToCollisionCheck : AllNodes)
		{
			FSlateRect CollisionBounds = NodeToCollisionCheck->GetBounds().ExtendBy(FMargin(0, TrackSpacing - 1));

			UE_LOG(LogKnotTrack, Log, TEXT("Collision check against %s | %s | %s"), *NodeToCollisionCheck->GetName(), *CollisionBounds.ToString(), *Point.ToString());

			if (NodeToCollisionCheck == SourcePin->GetOwningNode() || NodeToCollisionCheck == OtherPin->GetOwningNode())
			{
				UE_LOG(LogKnotTrack, Log, TEXT("\tSkipping node"));
				continue;
			}

			if (FBAUtils::LineRectIntersection(CollisionBounds, SourcePinPos, Point))
			{
				// UE_LOG(LogKnotTrack, Log, TEXT("\tFound collision"));
				bAnyCollision = true;
				break;
			}
		}

		for (FBAKnotTrackPtr OtherTrack : KnotTracks)
		{
			if (OtherTrack == Track)
			{
				continue;
			}

			// Possibly revert back to rect collision check
			if (FBAUtils::LineRectIntersection(OtherTrack->GetTrackBounds().ExtendBy(FMargin(0, TrackSpacing * 0.25f)), SourcePinPos, Point))
			{
				// UE_LOG(LogKnotTrack, Log, TEXT("Track %s colliding with %s"), *Track->ToString(), *OtherTrack->ToString());
				// UE_LOG(LogKnotTrack, Log, TEXT("\tStart %s End %s"), *SourcePinPos.ToString(), *Point.ToString());
				// UE_LOG(LogKnotTrack, Log, TEXT("\tRect %s"), *MyTrackBounds.ToString());
				bAnyCollision = true;
				break;
			}
		}

		if (!bAnyCollision)
		{
			UE_LOG(LogKnotTrack, Log, TEXT("[%hs] Aligned track to pin %s"), __FUNCTION__, *SourcePin->GetName(true))
			Track->PinAlignedX = Point.X;
			Track->UpdateTrackHeight(SourcePinPos.Y);
			Track->PinToAlignTo = SourcePin;
			return true;
		}
	}

	return false;
}

bool FBAKnotTrackManager::DoesPinNeedTrack(FBAPinPtr Pin, const TArray<FBAPinPtr>& LinkedTo)
{
	if (LinkedTo.Num() == 0)
	{
		return false;
	}

	// if the pin is linked to multiple linked nodes, we need a knot track
	if (LinkedTo.Num() > 1)
	{
		// UE_LOG(LogKnotTrack, Log, TEXT("Multiple linked to?"));
		return true;
	}

	// otherwise the pin is linked to exactly 1 node, run a collision check
	FBAPinPtr OtherPin = LinkedTo[0];

	// need pin if there are any collisions
	return AnyCollisionBetweenPins(Pin, OtherPin);
}

bool FBAKnotTrackManager::DoLinksNeedTrack(const TArray<FBAPinLink>& Links)
{
	if (Links.Num() == 0)
	{
		return false;
	}

	if (Links.Num() > 1)
	{
		return true;
	}

	return AnyCollisionBetweenPins(Links[0].From, Links[0].To);
}

bool FBAKnotTrackManager::AnyCollisionBetweenPins(FBAPinPtr Pin, FBAPinPtr OtherPin)
{
	const FVector2D PinPos = Pin->GetPos();
	const FVector2D OtherPinPos = OtherPin->GetPos();

	// UE_LOG(LogKnotTrack, Log, TEXT("Checking pins %s %s"), *FBAUtils::GetPinName(Pin), *FBAUtils::GetPinName(OtherPin));

	return NodeCollisionBetweenLocation(PinPos, OtherPinPos, { Pin->GetOwningNode(), OtherPin->GetOwningNode() });
}

bool FBAKnotTrackManager::NodeCollisionBetweenLocation(FVector2D Start, FVector2D End, TSet<FBANodePtr> IgnoredNodes)
{
	TArray<FBANodePtr> FormattedNodes = GetLayout()->MasterGraph->Nodes;

	for (FBANodePtr NodeToCollisionCheck : FormattedNodes)
	{
		if (IgnoredNodes.Contains(NodeToCollisionCheck))
		{
			continue;
		}

		FSlateRect NodeBounds = NodeToCollisionCheck->GetBounds();
		if (FBAUtils::LineRectIntersection(NodeBounds, Start, End))
		{
#if DEBUG_DRAW_TRACKS
			GraphHandler->GetGraphOverlay()->DrawBounds(NodeBounds, FLinearColor::Red);
			GraphHandler->GetGraphOverlay()->DrawLine(Start, End, FLinearColor::Green);
#endif
			// UE_LOG(LogKnotTrack, Log, TEXT("\tNode collision! %s"), *GetNameSafe(NodeToCollisionCheck));
			return true;
		}
	}

	return false;
}

TSet<UEdGraphNode*> FBAKnotTrackManager::GetSpawnedKnotNodes()
{
	TSet<UEdGraphNode*> OutNodes;
	for (TWeakObjectPtr<UEdGraphNode> Node : SpawnedKnotNodes)
	{
		if (Node.IsValid())
		{
			OutNodes.Add(Node.Get());
		}
	}

	return OutNodes;
}

void FBAKnotTrackManager::Reset()
{
	KnotNodesSet.Reset();
	KnotTracks.Reset();
	// KnotNodeOwners.Reset();
}

void FBAKnotTrackManager::AddKnotNodesToComments()
{
	FCommentHandler* CommentHandler = Formatter->GetCommentHandler();
	if (!CommentHandler)
	{
		return;
	}

	if (CommentHandler->GetComments().Num() == 0)
	{
		return;
	}

	for (auto Track : KnotTracks)
	{
		TArray<UEdGraphNode*> TrackNodes;
		AlgoBA::TransformAccumulate(Track->GetNodes(), TrackNodes, &FBANode::NodeObj, TAdd());

		// process tracks with only 1 knot in AddNomadKnotsIntoComments
		if (Track->SpawnedKnots.Num() <= 1)
		{
			continue;
		}

		for (const auto& Comment : CommentHandler->GetComments())
		{
			TArray<UEdGraphNode*> Containing = CommentHandler->GetNodesUnderComments(Comment);

			// Add knots if all the track nodes are contained in the comment
			if (FBAUtils::DoesArrayContainsAllItems(Containing, TrackNodes))
			{
				FCommentNodeSet NodesUnderComment = Comment->GetNodesUnderComment();

				for (const FBAKnot& Creation : Track->SpawnedKnots)
				{
					if (auto CreatedKnot = Creation.GraphNode.Get())
					{
						if (!NodesUnderComment.Contains(CreatedKnot))
						{
							UE_LOG(LogKnotTrack, Log, TEXT("[%hs] Added %s into %s"), __FUNCTION__, *FBAUtils::GetNodeName(CreatedKnot), *FBAUtils::GetNodeName(Comment));

							// if we should ignore this comment then don't track the comment in the comment handler
							if (CommentHandler->ShouldIgnoreComment(Comment))
							{
								Comment->AddNodeUnderComment(CreatedKnot);
							}
							else
							{
								CommentHandler->AddNodeIntoComment(Comment, CreatedKnot);
							}

							// KnotsInComments.Add(Creation->CreatedKnot);
						}
					}
				}
			}
		}
	}
}

void FBAKnotTrackManager::AddNomadKnotsIntoComments()
{
	FCommentHandler* CommentHandler = Formatter->GetCommentHandler();
	if (!CommentHandler)
	{
		return;
	}

	for (UEdGraphNode_Comment* Comment : CommentHandler->GetComments())
	{
		const FMargin Padding = CommentHandler->GetCommentPadding(Comment);
		FSlateRect CommentBounds = CommentHandler->GetCommentBounds(Comment).InsetBy(Padding);

		if (UBASettings_Advanced::HasDebugSetting("DebugNomadKnots"))
		{
			GraphHandler->GetGraphOverlay()->DrawBounds(CommentBounds);
		}

		for (FBAKnotTrackPtr Track : KnotTracks)
		{
			if (Track->SpawnedKnots.Num() != 1)
			{
				continue;
			}

			const FBAKnot& Creation = Track->SpawnedKnots[0];
			if (UEdGraphNode* CreatedKnotNode = Creation.GraphNode.Get())
			{
				// Add nomad knots if they are located in a comment
				const FVector2D KnotPos(CreatedKnotNode->NodePosX, CreatedKnotNode->NodePosY);
				if (CommentBounds.ContainsPoint(KnotPos))
				{
					UE_LOG(LogKnotTrack, Log, TEXT("[%hs] Added %s into %s"), __FUNCTION__, *FBAUtils::GetNodeName(CreatedKnotNode), *FBAUtils::GetNodeName(Comment));
					CommentHandler->AddNodeIntoComment(Comment, CreatedKnotNode);

					if (UBASettings_Advanced::HasDebugSetting("DebugNomadKnots"))
					{
						GraphHandler->GetGraphOverlay()->DrawBounds(FSlateRect(KnotPos.X - 10.0f, KnotPos.Y - 10.0f, KnotPos.X + 10.0f, KnotPos.Y + 10.0f));
					}
				}
			}
		}
	}
}

void FBAKnotTrackManager::SpawnKnotNodes()
{
	UE_LOG(LogKnotTrack, Log, TEXT("Spawning knot nodes"))
	const float TrackIndexOffset = 12.0f;
	const UBAGraphSchema& Schema = GraphHandler->GetSchema();

	UEdGraph* Graph = GraphHandler->GetFocusedEdGraph();

	for (auto Group : TrackGroups)
	{
		UE_LOG(LogKnotTrack, Log, TEXT("Group %s"), *Group->Tracks[0]->ToString());
		for (int TrackIndex = 0; TrackIndex < Group->Tracks.Num(); ++TrackIndex)
		{
			TSharedPtr<FBAKnotTrack> Track = Group->Tracks[TrackIndex];
			int Padding = Track->IsExecTrack()
				? NodePadding.X + UBASettings::Get().BlueprintExecutionKnotSettings.KnotXOffset
				: PinPadding.X + UBASettings::Get().BlueprintParameterKnotSettings.KnotXOffset;

			if (Track->bIsLoopingTrack)
			{
				if (Track->IsExecTrack())
				{
					Padding = UBASettings::Get().BlueprintExecutionKnotSettings.LoopingOffset.X;
				}
				else
				{
					Padding = UBASettings::Get().BlueprintParameterKnotSettings.LoopingOffset.X;
				}
			}

			bool bPushKnotNodes = false; // TODO add a setting for this
			if (bPushKnotNodes)
			{
				auto TrackOffset = ((Group->Tracks.Num() - 1) - TrackIndex) * TrackIndexOffset;
				Padding = FMath::Min(Padding + TrackOffset, NodePadding.X);
			}

			// invert padding for looping exec track or non-looping param track
			if ((Track->bIsLoopingTrack && Track->IsExecTrack()) || (!Track->bIsLoopingTrack && !Track->IsExecTrack()))
			{
				Padding *= -1;
			}

			// generate knots from the track
			TArray<FBAKnot>& Knots = Track->SpawnedKnots;

			const bool bIsAlignedTrack = Track->PinToAlignTo.IsValid();

			{
				// if (!bIsAlignedToPin)
				{
					// TODO use bIsAlignedToPin?
					// int32 Pos = bIsAlignedToPin
					// ? Track->PinAlignedX
					// : Track->GetRootPin()->GetPos().X - Padding;

					if (!bIsAlignedTrack || Track->GetRootPin() != Track->PinToAlignTo)
					{
						Knots.Add(FBAKnot(Track, {}, Track->GetRootPin()->GetPos().X - Padding));
					}
				}
				// else
				// {
				// 	UE_LOG(LogKnotTrack, Log, TEXT("Found aligned track? %s"), *Track->ToString())
				// }

				for (int i = 0; i < Track->Links.Num(); ++i)
				{
					int32 Pos = Track->Links[i].To->GetPos().X + Padding; 

					Knots.Add(FBAKnot(Track, Track->Links[i], Pos));
					UE_LOG(LogKnotTrack, Log, TEXT("Track linked to knot %s %d"), *Track->Links[i].ToString(), Pos);
				}
			}

			for (auto Knot : Knots)
			{
				FVector2D Point(Knot.Pos, Track->GetTrackHeight());
#if DEBUG_DRAW_TRACKS
				GraphHandler->GetGraphOverlay()->DrawBounds(FSlateRect::FromPointAndExtent(Point, FVector2D(10)), FLinearColor::Green);
#endif
			}

			if (!UBASettings_Advanced::HasDebugSetting("SpawnKnots"))
			{
				UEdGraphPin* LastPin = Track->GetRootPin()->GetObj();
				TOptional<int32> PrevKnotPos; 
				for (FBAKnot& Knot : Knots)
				{
					UEdGraphPin* ToPin = nullptr; 
					if (Knot.Link.IsSet())
					{
						ToPin = Knot.Link->To->GetObj();

						UEdGraphPin* FromPin = Knot.Link->From->GetObj();
						Schema.BreakLinkDirect(FromPin, ToPin);
					}

					UE_LOG(LogKnotTrack, Log, TEXT("Spawning knot node %s (Aligned: %d)"), *(Knot.Link.IsSet() ? Knot.Link->ToString() : "None Link"), bIsAlignedTrack);

					bool bSkipCreation = false;

					if (PrevKnotPos.IsSet())
					{
						if (FMath::Abs(PrevKnotPos.GetValue() - Knot.Pos) < 50)
						{
							bSkipCreation = true;
							UE_LOG(LogKnotTrack, Log, TEXT("\tMerging too close"));
						}
					}

					if (bIsAlignedTrack)
					{
						if (ToPin == Track->PinToAlignTo->GetObj())
						{
							bSkipCreation = true;
							UE_LOG(LogKnotTrack, Log, TEXT("\tSkip aligned to"));
						}

						if (!Knot.Link.IsSet() && Track->PinToAlignTo == Track->GetRootPin())
						{
							bSkipCreation = true;
							UE_LOG(LogKnotTrack, Log, TEXT("\tSkip aligned root"));
						}
					}

					if (bSkipCreation)
					{
						if (LastPin)
						{
							Schema.MakeLinkDirect(LastPin, ToPin);
							UE_LOG(LogKnotTrack, Log, TEXT("\tMerging with previous"));
						}
						else
						{
							UE_LOG(LogKnotTrack, Log, TEXT("\tNo previous pin, skipping"));
						}
					}
					else
					{
						FVector2D Point(Knot.Pos, Track->GetTrackHeight());

						UEdGraphNode* NewKnot = Schema.MakeKnotNode(LastPin, ToPin, Point);
						check(NewKnot);
						auto [InPin, OutPin] = FBASchemaUtils::GetKnotNodePins(NewKnot);

						SpawnedKnotNodes.Add(NewKnot);
						Knot.GraphNode = NewKnot;

						if (Track->GetRootPin()->Direction == EGPD_Output)
						{
							LastPin = OutPin;
						}
						else
						{
							LastPin = InPin;
						}

						PrevKnotPos = Knot.Pos;
						UE_LOG(LogKnotTrack, Log, TEXT("\tMade new knot"));
					}
				}
			}
		}
	}
}

TSharedPtr<FBALayout> FBAKnotTrackManager::GetLayout()
{
	return Formatter->FinalLayout;
}

void FBAKnotTrackManager::MakeKnotTrack()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBAKnotTrackManager::MakeKnotTrack"), STAT_BAKnotTrackManager_MakeKnotTrack, STATGROUP_BA_EdGraphFormatter);
	const TArray<FBANodePtr> FormattedNodes = GetLayout()->MasterGraph->Nodes;

	const auto& NotFormatted = [FormattedNodes](FBAPinPtr Pin)
	{
		return !FormattedNodes.Contains(Pin->GetOwningNode());
	};

	// iterate across the pins of all nodes and determine if they require a knot track
	for (FBANodePtr MyNode : FormattedNodes)
	{
		// make tracks for input exec pins
		for (auto Dir : { EGPD_Input, EGPD_Output })
		{
			TArray<FBAKnotTrackPtr> PreviousTracks;
			for (FBAPinPtr MyPin : MyNode->GetPins(Dir))
			{
				if (MyPin->HasLinks())
				{
					if (MyPin->IsExecPin() && MyPin->Direction == EGPD_Input)
					{
						MakeTrack(MyPin, MyPin->GetLinks(), PreviousTracks);
					}
					else if (MyPin->IsParameterPin() && MyPin->Direction == EGPD_Output)
					{
						MakeTrack(MyPin, MyPin->GetLinks(), PreviousTracks);
					}
				}
			}
		}
	}

	// TODO paramater pins
	// for (FBANodePtr MyNode : FormattedNodes)
	// {
	// 	// make tracks for output parameter pins
	// 	TArray<FBAKnotTrackPtr> PreviousTracks;
	// 	for (FBAPinPtr MyPin : FBAUtils::GetParameterPins(MyNode, EGPD_Output))
	// 	{
	// 		TArray<FBAPinPtr> LinkedTo = MyPin->LinkedTo;
	// 		LinkedTo.RemoveAll(NotFormatted);
	// 		if (LinkedTo.Num() == 0)
	// 		{
	// 			continue;
	// 		}
	//
	// 		if (UBASettings::Get().ParameterWiringStyle == EBAWiringStyle::AlwaysMerge)
	// 		{
	// 			MakeParamTrack(MyPin, LinkedTo, PreviousTracks);
	// 		}
	// 		else
	// 		{
	// 			for (FBAPinPtr Pin : LinkedTo)
	// 			{
	// 				MakeParamTrack(MyPin, { Pin }, PreviousTracks);
	// 			}
	// 		}
	// 	}
	// }
}

FBAKnotTrackPtr FBAKnotTrackManager::MakeTrack(FBAPinPtr ParentPin, TArray<FBAPinLink> LinkedPins, TArray<FBAKnotTrackPtr>& PreviousTracks)
{
	bool bIsParamPin = ParentPin->IsParameterPin();

	FVector2D ParentPinPos = ParentPin->GetPos();
	FBANodePtr ParentNode = ParentPin->GetOwningNode();
	UE_LOG(LogKnotTrack, Log, TEXT("Making knot track for %s"), *GetNameSafe(ParentPin));
	for (FBAPinLink LinkedPin : LinkedPins)
	{
		UE_LOG(LogKnotTrack, Log, TEXT("\t%s"), *LinkedPin.ToString());
	}

	const bool bPreviousHasTrack = PreviousTracks.Num() > 0;

	for (int i = LinkedPins.Num() - 1; i >= 0; --i)
	{
		FBAPinLink PinLink = LinkedPins[i];

		const FVector2D LinkedPinPos = PinLink.To->GetPos();

		bool bIsLooping = bIsParamPin
			? LinkedPinPos.X < ParentPinPos.X
			: LinkedPinPos.X > ParentPinPos.X;

		float Dist = FMath::Abs(LinkedPinPos.X - ParentPinPos.X);

		// only make looping tracks for exec nodes
		if (ParentPin->GetOwningNode()->IsExecNode())
		{
			if (bIsLooping && Dist > 200)
			{
				UE_LOG(LogKnotTrack, Log, TEXT("Make looping track"))
				auto TrackPins = { PinLink };
				FBAKnotTrackPtr KnotTrack = MakeShared<FBAKnotTrack>(this, TrackPins, true);
				SetTrackHeight(KnotTrack);
				KnotTracks.Add(KnotTrack);

				LinkedPins.RemoveAt(i);
				continue;
			}
		}

		// remove no collision
		if (!bPreviousHasTrack && !AnyCollisionBetweenPins(PinLink.From, PinLink.To))
		{
			LinkedPins.RemoveAt(i);
		}
	}

	if (LinkedPins.Num() == 0)
	{
		return nullptr;
	}

	FBAKnotTrackPtr Track = MakeShared<FBAKnotTrack>(this, LinkedPins, false);
	if (!TryAlignTrackToEndPins(Track, GetLayout()->MasterGraph->Nodes))
	{
		SetTrackHeight(Track);
	}

	FSlateRect Bounds = Track->GetTrackBounds();
	FVector2D LastPinPos = Track->GetLastPin()->GetPos();
 
	const float DistY = FMath::Abs(ParentPinPos.Y - LastPinPos.Y);

	// skip if we are the same height as our parent pin and we only have 1 pin
	// const bool bHeightDistance = DistY > 5.0f; 

	// skip the pin distance check if we are expanding by height
	const bool bPinReallyFar = Bounds.GetSize().X >= UBASettings::Get().KnotNodeDistanceThreshold;// && !UBASettings::Get().bExpandNodesByHeight;

	const bool bPinNeedsTrack = DoLinksNeedTrack(LinkedPins);

	// const FVector2D ToLast = LastPinPos - ParentPinPos;
	// const bool bTooSteep = FMath::Abs(ToLast.Y) / FMath::Abs(ToLast.X) >= 2.75f;
	// if (bTooSteep)
	// {
	// 	return nullptr;
	// }

	// UE_LOG(LogKnotTrack, Log, TEXT("Need reroute: %d %d %d %d %f %f"), bSkipSameHeight, bPinReallyFar, bPreviousHasTrack, bPinNeedsTrack, DistX, DistY);

	const bool bNeedsReroute = bPinNeedsTrack || bPreviousHasTrack;// || bPinReallyFar;
	UE_LOG(LogKnotTrack, Log, TEXT("Checking track Needs: %d Prev: %d %d %s"), bPinNeedsTrack, bPreviousHasTrack, PreviousTracks.Num(), *Track->ToString());
	if (!bNeedsReroute)
	{
		return nullptr;
	}

	KnotTracks.Add(Track);

	PreviousTracks.Add(Track);

	return Track;
}

void FBAKnotTrackManager::MakeTrackGroups(TArray<FBAKnotTrackPtr> Tracks)
{
	// we are popping from the list, so everything is sorted backwards
	const auto TrackSort = [](const FBAKnotTrackPtr& A, const FBAKnotTrackPtr& B)
	{
		if (A->bIsLoopingTrack != B->bIsLoopingTrack)
		{
			return A->bIsLoopingTrack < B->bIsLoopingTrack;
		}

		// highest track height
		if (A->GetTrackHeight() != B->GetTrackHeight())
		{
			return A->GetTrackHeight() > B->GetTrackHeight();
		}

		// exec track first
		if (A->IsExecTrack() != B->IsExecTrack())
		{
			return A->IsExecTrack() < B->IsExecTrack();
		}

		// right most first
		auto LeftPinA = A->bIsLoopingTrack ? A->GetRootPin() : A->GetLastPin();
		auto LeftPinB = A->bIsLoopingTrack ? A->GetRootPin() : A->GetLastPin();
		auto LastA = LeftPinA->GetPos().X;
		auto LastB = LeftPinB->GetPos().X;
		if (LastA != LastB)
		{
			return A->IsExecTrack() ? LastA < LastB : LastA > LastB;
		}

		// smallest width first / looping pins: largest first
		const float WidthA = A->GetTrackBounds().GetSize().X;
		const float WidthB = B->GetTrackBounds().GetSize().X;
		if (WidthA != WidthB)
		{
			return A->bIsLoopingTrack
				? WidthA < WidthB
				: WidthA > WidthB;
		}

		return true;
	};
	Tracks.Sort(TrackSort);

	TArray<FBAKnotTrackPtr> PendingTracks = Tracks;

	UE_LOG(LogKnotTrack, Log, TEXT("BEGIN LOG TRACKS %d"), Tracks.Num());
	for (FBAKnotTrackPtr T : Tracks)
	{
		UE_LOG(LogKnotTrack, Log, TEXT("\t%s"), *T->ToString());
	}
	UE_LOG(LogKnotTrack, Log, TEXT("END LOG TRACKS %d"), Tracks.Num());

	while (PendingTracks.Num() > 0)
	{
		FBAKnotTrackPtr RootTrack = PendingTracks.Pop();
		TSharedRef<FBATrackGroup> Group = MakeShared<FBATrackGroup>();
		Group->Tracks.Add(RootTrack);
		RootTrack->TrackGroup = Group;
		TrackGroups.Add(Group);

		UE_LOG(LogKnotTrack, Log, TEXT("Making group %s"), *RootTrack->ToString())
		for (int i = PendingTracks.Num() - 1; i >= 0; --i)
		{
			FBAKnotTrackPtr NextTrack = PendingTracks[i];
			UE_LOG(LogKnotTrack, Log, TEXT("\tChecking against track %s"), *NextTrack->ToString());

			// group looping tracks by their top branch
			if (RootTrack->bIsLoopingTrack)
			{
				if (NextTrack->TopBranch != RootTrack->TopBranch)
				{
					UE_LOG(LogKnotTrack, Log, TEXT("\t\tSkipping different branch"));
					continue;
				}
			}
			else // group regular tracks by their bottom branch
			{
				if (NextTrack->TopBranch != RootTrack->TopBranch)
				{
					UE_LOG(LogKnotTrack, Log, TEXT("\t\tSkipping different branch"));
					continue;
				}
			}

			FSlateRect NextTrackBounds = NextTrack->GetTrackBounds();
			FSlateRect GroupBounds = Group->GetBounds();
			if (FSlateRect::DoRectanglesIntersect(NextTrackBounds, GroupBounds))
			{
#if DEBUG_DRAW_TRACKS
				GraphHandler->GetGraphOverlay()->DrawBounds(GroupBounds, FLinearColor::Green);
				GraphHandler->GetGraphOverlay()->DrawBounds(NextTrackBounds, FLinearColor::Red);
#endif
				PendingTracks.RemoveAt(i);
				Group->Tracks.Add(NextTrack);
				NextTrack->TrackGroup = Group;
				UE_LOG(LogKnotTrack, Log, TEXT("\t\tTrack intersects! Adding to group"));
			}
		}

		Group->AssignTracks();
		UE_LOG(LogKnotTrack, Log, TEXT("[%hs] Made group %s"), __FUNCTION__, *Group->ToString());
		for (FBAKnotTrackPtr T : Group->Tracks)
		{
			UE_LOG(LogKnotTrack, Log, TEXT("\t%s"), *T->ToString());
		}

		if (!BA_DEBUG("SkipTrackCollision"))
		{
			CollideTrackGroups(Group, PendingTracks);
		}
	}
}

void FBAKnotTrackManager::CollideTrackGroups(TSharedPtr<FBATrackGroup> Group, const TArray<FBAKnotTrackPtr>& PendingTracks)
{
	TArray<FBANodePtr> NodePool = GetLayout()->MasterGraph->Nodes;

	FSlateRect GroupBounds = Group->GetBounds();

	TSharedPtr<FBAKnotTrack> RootTrack = Group->Tracks[0];

	if (Group->Tracks[0]->bIsLoopingTrack)
	{
		GroupBounds = GroupBounds.ExtendBy(FMargin(0, 0, 0, TrackSpacing * 2)); 

		// if we are a looping track, instead of colliding just push the entire branch down
		FBATreeBranchPtr TopTrackBranch = RootTrack->TopBranch;
		float Delta = GroupBounds.Bottom - TopTrackBranch->GetBranchBounds().Top;
		MoveBranchAndTracks(TopTrackBranch, Delta);
		UE_LOG(LogKnotTrack, Warning, TEXT("[%hs] Moved looping track group %s by %f"), __FUNCTION__, *Group->ToString(), Delta);
		return;
	}

	GroupBounds = GroupBounds.ExtendBy(FMargin(0, 0, 0, NodePadding.Y));

	// run a collision check against other branches, then move colliding branches down
	for (auto Branch : GetLayout()->Tree.Branches)
	{
		FSlateRect BranchBounds = Branch->GetBranchBounds();

		if (Branch->GetExpandedBranches().Contains(RootTrack->BottomBranch))
		{
			continue;
		}

		if (Branch->GetExpandedBranches().Contains(RootTrack->TopBranch))
		{
			continue;
		}

		if (FSlateRect::DoRectanglesIntersect(BranchBounds, GroupBounds))
		{
			float Delta = GroupBounds.Bottom - BranchBounds.Top;

			// move the node branch down
			MoveBranchAndTracks(Branch, Delta);

			UE_LOG(LogKnotTrack, Warning, TEXT("[%hs] Collision branch: %s and track group: %s"), __FUNCTION__, *Branch->ToString(), *Group->ToString());

#if DEBUG_DRAW_TRACKS
			GraphHandler->GetGraphOverlay()->DrawBounds(GroupBounds, FLinearColor::Green);
			GraphHandler->GetGraphOverlay()->DrawBounds(BranchBounds, FLinearColor::Red);
#endif
		}
	}
}

void FBAKnotTrackManager::MoveBranchAndTracks(FBATreeBranchPtr Branch, float Delta)
{
	TSet<FBATreeBranchPtr> BranchesToMove;
	Branch->GetVerticallyRelatedBranches(BranchesToMove);

	UE_LOG(LogKnotTrack, Warning, TEXT("[%hs] Moved branch %s by %f"), __FUNCTION__, *Branch->ToString(), Delta);

	TSet<FBAKnotTrackPtr> AdjustedTracks;
	for (auto BranchToMove : BranchesToMove)
	{
		for (FBAGroupNodePtr Node : BranchToMove->BranchNodes)
		{
			Node->MoveY(Delta);
			UE_LOG(LogKnotTrack, Log, TEXT("[%hs] Moving Branch: %s by %f"), __FUNCTION__, *Node->NodeName, Delta);

			for (auto NodeObj : Node->Nodes)
			{
				if (TSet<FBAKnotTrackPtr>* Tracks = TrackRelatedNodes.Find(NodeObj))
				{
					for (auto Track : *Tracks)
					{
						if (AdjustedTracks.Contains(Track))
						{
							continue;
						}

						// skip if it has already been placed in a track group - we only want to move pending tracks
						if (Track->TrackGroup.IsValid())
						{
							continue;
						}

						UE_LOG(LogKnotTrack, Warning, TEXT("[%hs] Moving Related Track: %s by %f"), __FUNCTION__, *Track->ToString(), Delta);
						Track->TrackHeight += Delta;
					}
				}
			}
		}
	}
}


void FBAKnotTrackManager::PrintKnotTracks()
{
	// UE_LOG(LogKnotTrack, Log, TEXT("### All Knot Tracks"));
	// for (FBAKnotTrackPtr Track : KnotTracks)
	// {
	// 	FString Aligned = Track->GetPinToAlignTo() != nullptr ? FString("True") : FString("False");
	// 	FString Looping = Track->bIsLoopingTrack ? FString("True") : FString("False");
	// 	UE_LOG(LogKnotTrack, Log, TEXT("\tKnot Tracks (%d) %s | %s | Aligned %s (%s) | Looping %s"),
	// 		Track->KnotCreations.Num(),
	// 		*FBAUtils::GetPinName(Track->GetRootPin(), true),
	// 		*FBAUtils::GetPinName(Track->GetLastPin(), true),
	// 		*Aligned, *FBAUtils::GetPinName(Track->GetPinToAlignTo(), true),
	// 		*Looping);
	//
	// 	for (TSharedPtr<FBAKnotNode> Elem : Track->KnotCreations)
	// 	{
	// 		if (auto MyPin = Elem->PinToConnectToHandle.GetPin())
	// 		{
	// 			UE_LOG(LogKnotTrack, Log, TEXT("\t\t%s %s"), *FBAUtils::GetPinName(MyPin, true), *Elem->KnotPos.ToString());
	// 		}
	//
	// 		for (auto PinHandle : Elem->PinHandlesToConnectTo)
	// 		{
	// 			if (auto MyPin = PinHandle.GetPin())
	// 			{
	// 				UE_LOG(LogKnotTrack, Log, TEXT("\t\t\t%s"), *FBAUtils::GetPinName(MyPin, true));
	// 			}
	// 		}
	// 	}
	// }
}

