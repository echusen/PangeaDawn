// Copyright fpwong. All Rights Reserved.

#include "BAKnotTrack.h"

#include "BAFormatter_Tree.h"
#include "BAKnotTrackManager.h"
#include "BALayout.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistUtils.h"
#include "K2Node_Knot.h"
#include "BlueprintAssistFormatters/FormatterInterface.h"

DEFINE_LOG_CATEGORY(LogKnotTrack);

FBAKnotTrack::FBAKnotTrack(
	FBAKnotTrackManager* InManager,
	const TArray<FBAPinLink>& InLinkedTo,
	bool bInIsLoopingTrack)
		: Manager(InManager)
		, bIsLoopingTrack(bInIsLoopingTrack)
{
	SetLinks(InLinkedTo);
}

FBAPinPtr FBAKnotTrack::GetRootPin()
{
	return Links[0].From;
}

FBAPinPtr FBAKnotTrack::GetLastPin()
{
	return Links.Last().To;
}

float FBAKnotTrack::GetTrackHeight()
{
	if (PinToAlignTo)
	{
		return PinToAlignTo->GetPos().Y;
	}

	return TrackHeight;
}

void FBAKnotTrack::SetLinks(TArray<FBAPinLink> InLinks)
{
	Links = InLinks;
	check(Links.Num() > 0);
	for (FBAPinLink Link : Links)
	{
		UE_LOG(LogKnotTrack, Log, TEXT("LINK %s"), *Link.ToString());
	}

	Links.Sort([](const FBAPinLink& A, const FBAPinLink& B)
	{
		const FVector2D PosA = A.To->GetPos();
		const FVector2D PosB = B.To->GetPos();
		if (PosA.X != PosB.X)
		{
			return A.GetDirection() == EGPD_Input ? PosA.X > PosB.X : PosA.X < PosB.X;
		}

		return PosA.Y < PosB.Y;
	});

	auto LastNode = Links.Last().GetToNode();
	UE_LOG(LogKnotTrack, Log, TEXT("LastNode %s"), *GetNameSafe(LastNode));
	UE_LOG(LogKnotTrack, Log, TEXT("RootPin %s"), *GetNameSafe(GetRootPin()));
	UE_LOG(LogKnotTrack, Log, TEXT("RootNode %s"), *GetNameSafe(GetRootPin()->GetOwningNode()));

	auto LastGroupNode = GetGroupGraph()->FindNode(LastNode);
	auto RootGroupNode = GetGroupGraph()->FindNode(GetRootPin()->GetOwningNode());
	check(LastGroupNode);
	check(RootGroupNode);

	LastBranch = LastGroupNode->GetBranch();
	FirstBranch = RootGroupNode->GetBranch();

	// select the highest branch
	bool bLastBranchTop = LastBranch->GetBranchBounds().Top < FirstBranch->GetBranchBounds().Top;
	TopBranch = bLastBranchTop ? LastBranch : FirstBranch;
	BottomBranch = bLastBranchTop ? FirstBranch : LastBranch;
}

FSlateRect FBAKnotTrack::GetTrackBounds()
{
	const int TrackSpacing = UBASettings::Get().BlueprintKnotTrackSpacing;
	const float LocalTrackY = GetTrackHeight();
	// const float LastPinX = FBAUtils::GetPinPos(GraphHandler, GetLastPin()).X;
	const float TrackXLeft = FMath::Min(GetRootPin()->GetPos().X, GetLastPin()->GetPos().X) + 5;
	const float TrackXRight = FMath::Max(GetRootPin()->GetPos().X, GetLastPin()->GetPos().X) - 5;

	return FSlateRect(FVector2D(TrackXLeft, LocalTrackY - (TrackSpacing - 1) * 0.5f),
					FVector2D(TrackXRight, LocalTrackY + (TrackSpacing - 1) * 0.5f));
}

bool FBAKnotTrack::IsFloatingTrack()
{
	const bool bSameAsParentPin = GetTrackHeight() != GetRootPin()->GetPos().Y;
	const bool bSameAsLastPin = GetTrackHeight() != GetLastPin()->GetPos().Y;
	return bSameAsParentPin && bSameAsLastPin;
}

void FBAKnotTrack::UpdateTrackHeight(const float NewTrackY)
{
	TrackHeight = NewTrackY;
	UE_LOG(LogKnotTrack, Log, TEXT("%s: Set track height %f"), *ToString(), TrackHeight);
}

TSet<FBANodePtr> FBAKnotTrack::GetNodes()
{
	TSet<FBANodePtr> OutNodes;
	OutNodes.Add(GetRootPin()->GetOwningNode());

	for (auto& PinHandle : Links)
	{
		OutNodes.Add(PinHandle.GetToNode());
	}

	return OutNodes;
}

bool FBAKnotTrack::DoesTrackOverlapNode(FBANodePtr Node)
{
	const FSlateRect Bounds = GetTrackBounds();

	return FBAUtils::LineRectIntersection(
		Node->GetBounds(),
		Bounds.GetTopLeft(),
		Bounds.GetBottomRight());
}

bool FBAKnotTrack::HasPinToAlignTo()
{
	return PinToAlignTo.IsValid();
}

TArray<FBANodePtr> FBAKnotTrack::GetRelatedNodes()
{
	TArray<FBANodePtr> Nodes;
	for (auto Link : Links)
	{
		Nodes.Add(Link.GetToNode());
	}

	return Nodes;
}

FString FBAKnotTrack::ToString()
{
	return FString::Printf(TEXT("%s -> %s (Height: %f) (Align: %s) (Loop: %d)"),
							*GetNameSafe(GetRootPin(), true),
							*GetNameSafe(GetLastPin(), true),
							GetTrackHeight(),
							*GetNameSafe(PinToAlignTo),
							bIsLoopingTrack);
}

void FBAKnotTrack::SortCreationNodes()
{
	UE_LOG(LogTemp, Warning, TEXT("TODO sort creation node"))
	// if (bIsLoopingTrack)
	// {
	// 	return;
	// }
	//
	// const auto CreationSorter = [](TSharedPtr<FBAKnotNode> CreationA, TSharedPtr<FBAKnotNode> CreationB)
	// {
	// 	FBAPinPtr Pin = CreationA->PinToConnectToHandle.GetPin();
	//
	// 	if (FBAUtils::IsExecOrDelegatePin(Pin))
	// 	{
	// 		return CreationA->KnotPos.X > CreationB->KnotPos.X;
	// 	}
	//
	// 	return CreationA->KnotPos.X < CreationB->KnotPos.X;
	// };
	//
	// KnotCreations.StableSort(CreationSorter);
}

TSharedPtr<FBAGroupGraph> FBAKnotTrack::GetGroupGraph()
{
	return Manager->GetLayout()->GroupGraph;
}

bool FBAKnotTrack::IsExecTrack()
{
	return GetRootPin()->IsExecPin();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

TArray<FBANodePtr> FBATrackGroup::GetKnots() const
{
	// TODO get knots
	UE_LOG(LogTemp, Warning, TEXT("TODO get knots"))
	return {};
	// TArray<FBANodePtr> Knots;
	// for (const auto& Track : Tracks)
	// {
	// 	for (const auto& KnotNodeCreation : Track->KnotCreations)
	// 	{
	// 		if (KnotNodeCreation->CreatedKnot)
	// 		{
	// 			Knots.Add(Cast<UEdGraphNode>(KnotNodeCreation->CreatedKnot));
	// 		}
	// 	}
	// }

	// return Knots;
}

FSlateRect FBATrackGroup::GetBounds() const
{
	if (!Tracks.Num())
	{
		return FSlateRect();
	}


	// calc the track width
	TOptional<FSlateRect> Bounds;
	for (const auto& KnotNodeTrack : Tracks)
	{
		const FSlateRect TrackBounds = KnotNodeTrack->GetTrackBounds();
		Bounds = Bounds.IsSet()
			? Bounds->Expand(TrackBounds)
			: TrackBounds;
	}

	auto StartY = Tracks[0]->GetTrackHeight();

	float Height = Tracks.Num() * UBASettings::Get().BlueprintKnotTrackSpacing;
	return FSlateRect(Bounds->Left, StartY, Bounds->Right, StartY + Height);

	// TSharedPtr<FBAKnotTrack> StartHeight = Tracks[0]->GetTrackHeight();
	// for (int i = 1; i < Tracks.Num(); ++i)
	// {
	// 	Tracks[i]->UpdateTrackHeight(StartHeight)
	// }

	// return Bounds.Get(FSlateRect());
}

void FBATrackGroup::AssignTracks()
{
	float StartY = Tracks[0]->GetTrackHeight();
	for (int i = 0; i < Tracks.Num(); ++i)
	{
		TSharedPtr<FBAKnotTrack> Track = Tracks[i];
		Track->UpdateTrackHeight(StartY + UBASettings::Get().BlueprintKnotTrackSpacing * i);

		if (Tracks.Num() > 1)
		{
			Track->PinToAlignTo = nullptr;
		}
	}
}

void FBATrackGroup::OffsetGroup(float Amount)
{
	for (auto Track : Tracks)
	{
		Track->TrackHeight += Amount;
	}
}

TSet<FBANodePtr> FBATrackGroup::GetRootNodes() const
{
	TSet<FBANodePtr> RootNodes;
	for (auto KnotNodeTrack : Tracks)
	{
		RootNodes.Add(KnotNodeTrack->GetRootPin()->GetOwningNode());
		RootNodes.Add(KnotNodeTrack->GetLastPin()->GetOwningNode());
	}
	return RootNodes;
}

FString FBATrackGroup::ToString() const
{
	return FString::Printf(TEXT("TrackGroup (%d) %s"), Tracks.Num(), *Tracks[0]->ToString());
}
