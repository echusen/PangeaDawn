// Copyright fpwong. All Rights Reserved.

#include "BAGraphSnapshotManager.h"

#include "BlueprintAssistWidgets/BAAddSymbolMenu.h"
#include "Misc/LazySingleton.h"

TArray<TSharedPtr<FBAGraphSnapshotGroup>> FBAScopedSnapshotGroup::GlobalStack;

void FBAScopedSnapshotGroup::MakeLeafSnapshot(const FString& InDisplayName, FBAGroupGraphPtr Graph)
{
	GetLast()->MakeLeafSnapshot(InDisplayName, Graph);
}

FBAScopedSnapshotGroup::FBAScopedSnapshotGroup(const FString& GroupName, FBAGroupGraphPtr Graph)
{
	GroupGraph = Graph;
	FBAGraphSnapshotManager& Manager = FBAGraphSnapshotManager::Get();

	TSharedPtr<FBAGraphSnapshotGroup> PrevGroup = GlobalStack.IsEmpty() ? Manager.RootGroup : GlobalStack.Last();

	if (PrevGroup)
	{
		TSharedRef<FBAGraphSnapshotGroup> NewGroup = PrevGroup->MakeSubGroup(GroupName);
		if (Graph)
		{
			TSharedRef<FBAGraphSnapshot> NewSnapshot = MakeShared<FBAGraphSnapshot>(GroupName, Graph);
			NewGroup->Snapshot = NewSnapshot;
		}

		GlobalStack.Add(NewGroup);
		Group = NewGroup;
	}
}

void FBAScopedSnapshotGroup::Cleanup(bool bCreateSnapshot)
{
	if (!Group.IsValid())
	{
		return;
	}

	if (bCreateSnapshot)
	{
		// if we have no subgroups, make a dummy end snapshot
		if (Group->SubGroups.IsEmpty())
		{
			MakeLeafSnapshot("End", GroupGraph);
		}
	}

	while (!GlobalStack.IsEmpty())
	{
		TSharedPtr<FBAGraphSnapshotGroup> Next = GlobalStack.Pop();
		if (Next == Group)
		{
			break;
		}
	}

	Group.Reset();
}

void FBAScopedSnapshotGroup::Cancel()
{
	if (Group.IsValid() && Group->Parent.IsValid())
	{
		Group->Parent->SubGroups.Remove(Group);
	}

	Cleanup(false);
}

FBAGraphSnapshot::FBAGraphSnapshot(const FString& InDisplayName, FBAGroupGraphPtr Graph)
{
	DisplayName = InDisplayName;

	if (Graph.IsValid())
	{
		FBAGraphPtr FullGraph = Graph->FullGraph;
		for (FBAGroupNodePtr& GroupNode : Graph->Nodes)
		{
			for (FBANodePtr& GraphNode : GroupNode->Nodes)
			{
				auto NodePos = GroupNode->GetContainedNodePos(GraphNode);
				if (UEdGraphNode* NodeObj = GraphNode->NodeObj)
				{
					GraphNode->NodePosX = NodePos.X;
					GraphNode->NodePosY = NodePos.Y;

					Positions.Add(NodeObj, FVector2D(NodePos));
				}
			}
		}

		// it can be possible for nodes to not be inside the group graph, so add them now 
		// i.e parameters which haven't been processed yet
		for (FBANodePtr Node : FullGraph->Nodes)
		{
			if (UEdGraphNode* NodeObj = Node->NodeObj)
			{
				if (!Positions.Contains(NodeObj))
				{
					Positions.Add(NodeObj, FVector2D(Node->NodePosX, Node->NodePosY));
				}
			}
		}
	}
}

void FBAGraphSnapshot::ApplySnapshot()
{
	for (auto& [Node, Pos] : Positions)
	{
		if (Node.IsValid())
		{
			Node.Pin()->NodePosX = Pos.X;
			Node.Pin()->NodePosY = Pos.Y;
		}
	}
}

TSharedRef<FBAGraphSnapshotGroup> FBAGraphSnapshotGroup::MakeSubGroup(FString Name)
{
	TSharedRef<FBAGraphSnapshotGroup> NewGroup = MakeShared<FBAGraphSnapshotGroup>(Name, EType::Group);
	NewGroup->Parent = AsShared();
	SubGroups.Add(NewGroup);

	FBAGraphSnapshotManager::Get().OnSnapshotsChanged.Broadcast();
	return NewGroup;
}

void FBAGraphSnapshotGroup::MakeLeafSnapshot(const FString& InDisplayName, FBAGroupGraphPtr Graph)
{
	TSharedRef<FBAGraphSnapshotGroup> LeafGroup = MakeShared<FBAGraphSnapshotGroup>(InDisplayName, EType::Snapshot);
	LeafGroup->Snapshot = MakeShared<FBAGraphSnapshot>(InDisplayName, Graph);
	LeafGroup->Parent = AsShared();

	SubGroups.Add(LeafGroup);
	FBAGraphSnapshotManager::Get().OnSnapshotsChanged.Broadcast();
}

FBAGraphSnapshotManager& FBAGraphSnapshotManager::Get()
{
	return TLazySingleton<FBAGraphSnapshotManager>::Get();
}
