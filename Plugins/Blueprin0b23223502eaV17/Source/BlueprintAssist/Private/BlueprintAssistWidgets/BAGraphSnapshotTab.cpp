// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistWidgets/BAGraphSnapshotTab.h"

#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

SBAGraphSnapshotTab::SBAGraphSnapshotTab()
{
	FBAGraphSnapshotManager::Get().OnSnapshotsChanged.AddRaw(this, &SBAGraphSnapshotTab::HandleSnapshotsChanged);
}

SBAGraphSnapshotTab::~SBAGraphSnapshotTab()
{
	FBAGraphSnapshotManager::Get().OnSnapshotsChanged.RemoveAll(this);
}

void SBAGraphSnapshotTab::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FBAGraphSnapshotGroup>>)
			.TreeItemsSource(&TreeRoot)
			.OnGetChildren(this, &SBAGraphSnapshotTab::OnGetChildren)
			.OnGenerateRow(this, &SBAGraphSnapshotTab::OnGenerateRow)
			.OnSelectionChanged(this, &SBAGraphSnapshotTab::OnSelectionChanged)
		]
	];
}

void SBAGraphSnapshotTab::RegisterNomadTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			"GraphSnapshotTab",
			FOnSpawnTab::CreateLambda([](const FSpawnTabArgs& Args)
			{
				return SNew(SDockTab).TabRole(ETabRole::NomadTab)
					[
						SNew(SBAGraphSnapshotTab)
					];
			})
		)
		.SetDisplayName(FText::FromString("BA Graph Snapshot Tab"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void SBAGraphSnapshotTab::RefreshTree()
{
	TreeRoot.Empty();
	TreeRoot = FBAGraphSnapshotManager::Get().RootGroup->SubGroups;

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}

	// make all the items expanded
	TArray<TSharedPtr<FBAGraphSnapshotGroup>> FlatItems;
	GetAllItems(TreeRoot, FlatItems);

	for (TSharedPtr<FBAGraphSnapshotGroup> SnapshotTreeItem : FlatItems)
	{
		TreeView->SetItemExpansion(SnapshotTreeItem, true);
	}
}

void SBAGraphSnapshotTab::HandleSnapshotsChanged()
{
	if (!SnapshotDirtyHandle.IsValid())
	{
		SnapshotDirtyHandle = GEditor->GetTimerManager()->SetTimerForNextTick([&]()
		{
			RefreshTree();
			SnapshotDirtyHandle.Invalidate();
		});
	}
}

TSharedRef<ITableRow> SBAGraphSnapshotTab::OnGenerateRow(TSharedPtr<FBAGraphSnapshotGroup> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bIsGroup = Item->Type == FBAGraphSnapshotGroup::EType::Group;

	return SNew(STableRow<TSharedPtr<FBAGraphSnapshotGroup>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 2)
			[
				SNew(STextBlock).Text(FText::FromString(bIsGroup ? TEXT("📁") : TEXT("📸")))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
			[
				SNew(STextBlock).Text(FText::FromString(Item->DisplayName))
			]
		];
}

void SBAGraphSnapshotTab::OnGetChildren(TSharedPtr<FBAGraphSnapshotGroup> InParent, TArray<TSharedPtr<FBAGraphSnapshotGroup>>& OutChildren)
{
	OutChildren = InParent->SubGroups;
}

void SBAGraphSnapshotTab::OnSelectionChanged(TSharedPtr<FBAGraphSnapshotGroup> Item, ESelectInfo::Type SelectInfo)
{
	if (Item && Item->Snapshot.IsValid())
	{
		Item->Snapshot->ApplySnapshot();
	}
}

void SBAGraphSnapshotTab::GetAllItems(const TArray<TSharedPtr<FBAGraphSnapshotGroup>>& InItems, TArray<TSharedPtr<FBAGraphSnapshotGroup>>& OutItems) const
{
	for (const TSharedPtr<FBAGraphSnapshotGroup>& Item : InItems)
	{
		OutItems.Add(Item);

		if (!Item->SubGroups.IsEmpty())
		{
			GetAllItems(Item->SubGroups, OutItems);
		}
	}
}
