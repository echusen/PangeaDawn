// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAKnotTrack.h"

class FBALayout;
class FBAFormatter_Tree;
struct FCommentHandler;
struct FPinLink;

class FBAKnotTrackManager final
{
public:
	TSharedPtr<FBAFormatter_Tree> Formatter;
	TSharedPtr<FBAGraphHandler> GraphHandler;
	TSet<FBANodePtr> KnotNodesSet;
	TArray<FBAKnotTrackPtr> KnotTracks;
	TArray<TWeakObjectPtr<UEdGraphNode>> SpawnedKnotNodes;
	TMap<FBANodePtr, TSet<FBAKnotTrackPtr>> TrackRelatedNodes;
	// TMap<UK2Node_Knot*, FBANodePtr> KnotNodeOwners;
	// TSet<UK2Node_Knot*> PinAlignedKnots;
	// TSet<UK2Node_Knot*> KnotsInComments;

	// TMap<UK2Node_Knot*, TSharedPtr<FBATrackGroup>> KnotTrackGroup; 
	// TMap<UK2Node_Knot*, TSharedPtr<FBAKnotNode>> KnotCreationMap; 
	TArray<TSharedPtr<FBATrackGroup>> TrackGroups;

	FIntPoint PinPadding;
	FIntPoint NodePadding;
	int TrackSpacing;

	// TMap<FBANodePtr, TArray<FBANodePtr>> RelativeMapping;
	// TMap<TSharedPtr<FBAKnotNode>, FBANodePtr> RelativeCreationMapping;

	FBAKnotTrackManager() = default;
	void Init(TSharedPtr<FBAFormatter_Tree> InFormatter);

	void SetTrackHeight(FBAKnotTrackPtr Track);

	void FormatKnotNodes();
	void RemoveKnotNodes(const TArray<UEdGraphNode*>& NodeTree);
	const TSet<FBANodePtr>& GetCreatedKnotNodes() { return KnotNodesSet; }
	TSet<UEdGraphNode*> GetSpawnedKnotNodes();

	void Reset();

	// bool IsPinAlignedKnot(const UK2Node_Knot* KnotNode);
	// TSharedPtr<FBATrackGroup> GetKnotGroup(const UK2Node_Knot* KnotNode);

	// bool IsKnotInsideComment(const UK2Node_Knot* KnotNode) const { return KnotsInComments.Contains(KnotNode); }

	void AddKnotNodesToComments();
	void AddNomadKnotsIntoComments();

	void SpawnKnotNodes();

	TSharedPtr<FBALayout> GetLayout();

private:
	void MakeKnotTrack();

	FBAKnotTrackPtr MakeTrack(FBAPinPtr ParentPin, TArray<FBAPinLink> LinkedPins, TArray<FBAKnotTrackPtr>& PreviousTracks);

	// FBAKnotTrackPtr MakeParamTrack(FBAPinPtr ParentPin, TArray<FBAPinPtr> LinkedPins, TArray<FBAKnotTrackPtr>& PreviousTracks);

	void MakeTrackGroups(TArray<FBAKnotTrackPtr> Tracks);

	void CollideTrackGroups(TSharedPtr<FBATrackGroup> Group, const TArray<FBAKnotTrackPtr>& PendingTracks);

	void MoveBranchAndTracks(FBATreeBranchPtr Branch, float Delta);

	bool TryAlignTrackToEndPins(FBAKnotTrackPtr Track, const TArray<FBANodePtr>& AllNodes);

	bool DoesPinNeedTrack(FBAPinPtr Pin, const TArray<FBAPinPtr>& LinkedTo);
	bool DoLinksNeedTrack(const TArray<FBAPinLink>& Links);

	bool AnyCollisionBetweenPins(FBAPinPtr Pin, FBAPinPtr OtherPin);

	bool NodeCollisionBetweenLocation(FVector2D Start, FVector2D End, TSet<FBANodePtr> IgnoredNodes);

	void PrintKnotTracks();
};
