// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraph.h"
#include "BAKnotTrack.h"
#include "BATree.h"
#include "SGraphPin.h"

class FBAKnotTrackManager;
class UK2Node_Knot;
struct FFormatterInterface;
class FBAGraphHandler;

BLUEPRINTASSIST_API DECLARE_LOG_CATEGORY_EXTERN(LogKnotTrack, NoLogging, All);

using FBAKnotTrackPtr = TSharedPtr<struct FBAKnotTrack>;

struct FBAKnot
{
	FBAKnotTrackPtr Track;
	TOptional<FBAPinLink> Link;
	int32 Pos;
	TWeakObjectPtr<UEdGraphNode> GraphNode;
};

struct FBAKnotTrack
	: public TSharedFromThis<FBAKnotTrack>
{
	/* Pins */
	TArray<FBAPinLink> Links;
	FBAPinPtr GetRootPin();
	FBAPinPtr GetLastPin();
	FBATreeBranchPtr TopBranch;
	FBATreeBranchPtr BottomBranch;
	FBATreeBranchPtr FirstBranch;
	FBATreeBranchPtr LastBranch;
	TSharedPtr<struct FBATrackGroup> TrackGroup;
	FBAKnotTrackManager* Manager;

	/* Determines track height */
	FBAPinPtr PinToAlignTo;
	float TrackHeight = 0.0f;

	/* Determine track width */
	FVector2D TrackStart;
	FVector2D TrackEnd;
	int32 PinAlignedX = 0;

	float GetTrackHeight();
	FSlateRect GetTrackBounds();

	TSharedPtr<FBAGraphHandler> GraphHandler;

	TArray<FBAKnot> SpawnedKnots;
	bool bIsLoopingTrack = false;

	FBAKnotTrack(
		FBAKnotTrackManager* InManager,
		const TArray<FBAPinLink>& InLinkedTo,
		bool bInIsLoopingTrack);

	void SetLinks(TArray<FBAPinLink> InLinks);

	bool IsFloatingTrack();

	void UpdateTrackHeight(float NewTrackY);

	TSet<FBANodePtr> GetNodes();

	bool DoesTrackOverlapNode(FBANodePtr Node);

	bool HasPinToAlignTo();

	TArray<FBANodePtr> GetRelatedNodes();

	FString ToString();

	// sort creation nodes by x pos
	void SortCreationNodes();

	TSharedPtr<FBAGroupGraph> GetGroupGraph();

	bool IsExecTrack();

private:
};

using FBATrackGroupPtr = TSharedPtr<struct FBATrackGroup>;
struct BLUEPRINTASSIST_API FBATrackGroup
	: public TSharedFromThis<FBATrackGroup>
{
	FBANodePtr ParentNode;
	TArray<TSharedPtr<FBAKnotTrack>> Tracks;

	bool bLooping = false;

	TArray<FBANodePtr> GetKnots() const;
	FSlateRect GetBounds() const;
	void AssignTracks();
	void OffsetGroup(float Amount);
	TSet<FBANodePtr> GetRootNodes() const;
	FString ToString() const;
};