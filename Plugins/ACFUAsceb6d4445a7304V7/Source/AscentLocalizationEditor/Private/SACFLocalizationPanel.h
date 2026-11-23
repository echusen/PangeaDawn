// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

class ULocalizationTarget;
class UACFLocalizationSubsystem;

class SACFLocalizationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFLocalizationPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SACFLocalizationPanel() override;

private:
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TargetCombo;
	TArray<TSharedPtr<FString>> TargetNames;
	TSharedPtr<FString> SelectedTargetName;
	TSharedPtr<SEditableTextBox> CultureTextBox;
	TSharedPtr<SEditableTextBox> GatherPathTextBox;
	TArray<TSharedPtr<FString>> GatherPaths;
	TSharedPtr<FString> SelectedGatherPath;
	TSharedPtr<SListView<TSharedPtr<FString>>> GatherPathList;
	TSharedPtr<SCheckBox> OverwriteCheckBox;
	TSharedPtr<STextBlock> WorkingText;
	TSharedPtr<SProgressBar> ProgressBar;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> ErrorText;
	TSharedPtr<SMultiLineEditableTextBox> LogText;
	TSharedPtr<STextBlock> AvailabilityText;

	FDelegateHandle StatusHandle;
	FDelegateHandle FinishedHandle;

	void RefreshTargets();
	void RefreshGatherPaths();
	FReply OnAddGatherPath();
	FReply OnRemoveGatherPath();
	TSharedRef<ITableRow> OnGenerateGatherPathRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
	ULocalizationTarget* GetSelectedTarget() const;
	UACFLocalizationSubsystem* GetSubsystem() const;
	void OnTranslateClicked();
	void OnCancelClicked();
	void RefreshAvailability();
	FReply OnRefreshClicked();
	EVisibility GetBusyVisibility() const;
	EVisibility GetErrorVisibility() const;
	bool IsTranslateEnabled() const;
	void RefreshJobView();
	FText GetLogText() const;
};
