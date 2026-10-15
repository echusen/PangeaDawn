// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGroupGraph.h"
#include "EdGraph/EdGraphNode.h"

class FBAGraphSnapshot
{
public:
	FBAGraphSnapshot(const FString& InDisplayName, FBAGroupGraphPtr Graph);

	FString DisplayName;
	TMap<TWeakObjectPtr<UEdGraphNode>, FVector2D> Positions;

	void ApplySnapshot();
};

class FBAGraphSnapshotGroup : public TSharedFromThis<FBAGraphSnapshotGroup>
{
public:
	enum class EType { Group, Snapshot };

	FString DisplayName;
	EType Type;
	TArray<TSharedPtr<FBAGraphSnapshotGroup>> SubGroups;
	TSharedPtr<FBAGraphSnapshot> Snapshot;
	TSharedPtr<FBAGraphSnapshotGroup> Parent;

	FBAGraphSnapshotGroup(FString InName, EType InType)
		: DisplayName(MoveTemp(InName))
		, Type(InType)
	{
	}

	TSharedRef<FBAGraphSnapshotGroup> MakeSubGroup(FString Name);

	void MakeLeafSnapshot(const FString& InDisplayName, FBAGroupGraphPtr Graph);

	void Reset()
	{
		Type = EType::Group;
		DisplayName.Empty();
		SubGroups.Empty();
		Snapshot.Reset();
	}
};

class FBAGraphSnapshotManager
{
public:
	static FBAGraphSnapshotManager& Get();

	FSimpleMulticastDelegate OnSnapshotsChanged;

	TSharedPtr<FBAGraphSnapshotGroup> RootGroup;

	FBAGraphSnapshotManager()
	{
		RootGroup = MakeShared<FBAGraphSnapshotGroup>("Root", FBAGraphSnapshotGroup::EType::Group);
	}

	void ClearSnapshots()
	{
		RootGroup->Reset();
	}
};

class FBAScopedSnapshotGroup : public TSharedFromThis<FBAScopedSnapshotGroup>
{
public:
	TSharedPtr<FBAGraphSnapshotGroup> Group;
	FBAGroupGraphPtr GroupGraph;

	static TArray<TSharedPtr<FBAGraphSnapshotGroup>> GlobalStack;

	static TSharedPtr<FBAGraphSnapshotGroup> GetLast() { return GlobalStack.Last(); }

	static void MakeLeafSnapshot(const FString& InDisplayName, FBAGroupGraphPtr Graph);

	FBAScopedSnapshotGroup(const FString& GroupName, FBAGroupGraphPtr Graph = nullptr);

	~FBAScopedSnapshotGroup()
	{
		Cleanup(true);
	}

	void Cleanup(bool bCreateSnapshot);

	void Cancel();
};
