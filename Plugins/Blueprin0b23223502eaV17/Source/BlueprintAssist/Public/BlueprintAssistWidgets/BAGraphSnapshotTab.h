// Copyright fpwong. All Rights Reserved.

#pragma once

#include "BlueprintAssistFormatters/BAGroupFormatter/BAGraphSnapshotManager.h"

#include "Widgets/Views/STreeView.h"

class SBAGraphSnapshotTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBAGraphSnapshotTab)
		{
		}

	SLATE_END_ARGS()

	SBAGraphSnapshotTab();
	virtual ~SBAGraphSnapshotTab() override;

	void Construct(const FArguments& InArgs);

	static void RegisterNomadTab();

private:
	TArray<TSharedPtr<FBAGraphSnapshotGroup>> TreeRoot;
	TSharedPtr<STreeView<TSharedPtr<FBAGraphSnapshotGroup>>> TreeView;
	FTimerHandle SnapshotDirtyHandle;

	void RefreshTree();
	void HandleSnapshotsChanged();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FBAGraphSnapshotGroup> Item, const TSharedRef<STableViewBase>& OwnerTable);

	void OnGetChildren(TSharedPtr<FBAGraphSnapshotGroup> InParent, TArray<TSharedPtr<FBAGraphSnapshotGroup>>& OutChildren);

	void OnSelectionChanged(TSharedPtr<FBAGraphSnapshotGroup> Item, ESelectInfo::Type SelectInfo);

	void GetAllItems(const TArray<TSharedPtr<FBAGraphSnapshotGroup>>& InItems, TArray<TSharedPtr<FBAGraphSnapshotGroup>>& OutItems) const;
};
