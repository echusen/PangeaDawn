// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SACFLocalizationPanel.h"
#include "ACFLocalizationSubsystem.h"
#include "ACFAssistantCompletion.h"
#include "LocalizationTargetTypes.h"
#include "Editor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SListView.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SACFLocalizationPanel"

void SACFLocalizationPanel::Construct(const FArguments& InArgs)
{
	RefreshTargets();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Yarin AI Localization"))
				.Font(FAppStyle::GetFontStyle("HeadingMedium"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Help",
					"Translates every FText gathered by the selected Unreal localization target into the culture you enter. "
					"Uses whichever provider Yarin is signed into: ChatGPT (Codex) or Claude."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
			[
				SAssignNew(AvailabilityText, STextBlock)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("TargetLabel", "Localization target"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(TargetCombo, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&TargetNames)
					.InitiallySelectedItem(SelectedTargetName)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewValue, ESelectInfo::Type)
					{
						SelectedTargetName = NewValue;
						RefreshGatherPaths();
					})
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
					{
						return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
					})
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return FText::FromString(SelectedTargetName.IsValid() ? *SelectedTargetName : TEXT("(none)"));
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Refresh", "Refresh"))
					.OnClicked(this, &SACFLocalizationPanel::OnRefreshClicked)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("CultureLabel", "Culture code"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(CultureTextBox, SEditableTextBox)
					.HintText(LOCTEXT("CultureHint", "fr, de, ja, pt-BR..."))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GatherLabel", "Asset folders to scan"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SNew(SBox)
				.MinDesiredHeight(72.0f)
				.MaxDesiredHeight(120.0f)
				[
					SAssignNew(GatherPathList, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&GatherPaths)
					.SelectionMode(ESelectionMode::Single)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type)
					{
						SelectedGatherPath = Item;
					})
					.OnGenerateRow(this, &SACFLocalizationPanel::OnGenerateGatherPathRow)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 8, 0)
				[
					SAssignNew(GatherPathTextBox, SEditableTextBox)
					.HintText(LOCTEXT("GatherHint", "Content/MyFolder"))
					.IsEnabled(this, &SACFLocalizationPanel::IsTranslateEnabled)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("AddFolder", "Add"))
					.IsEnabled(this, &SACFLocalizationPanel::IsTranslateEnabled)
					.OnClicked(this, &SACFLocalizationPanel::OnAddGatherPath)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("RemoveFolder", "Remove"))
					.IsEnabled(this, &SACFLocalizationPanel::IsTranslateEnabled)
					.OnClicked(this, &SACFLocalizationPanel::OnRemoveGatherPath)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
			[
				SAssignNew(OverwriteCheckBox, SCheckBox)
				[
					SNew(STextBlock).Text(LOCTEXT("Overwrite", "Overwrite existing translations"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 12)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Translate", "Translate"))
					.IsEnabled(this, &SACFLocalizationPanel::IsTranslateEnabled)
					.OnClicked_Lambda([this]()
					{
						OnTranslateClicked();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Cancel", "Cancel"))
					.Visibility(this, &SACFLocalizationPanel::GetBusyVisibility)
					.OnClicked_Lambda([this]()
					{
						OnCancelClicked();
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
			[
				SNew(SHorizontalBox)
				.Visibility(this, &SACFLocalizationPanel::GetBusyVisibility)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[
					SNew(SThrobber)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SAssignNew(WorkingText, STextBlock)
					.Text(LOCTEXT("Working", "WORKING..."))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.75f, 1.0f)))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SAssignNew(ProgressBar, SProgressBar)
				.Percent(0.0f)
				.Visibility(this, &SACFLocalizationPanel::GetBusyVisibility)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
			[
				SAssignNew(StatusText, STextBlock)
				.Text(LOCTEXT("IdleStatus", "Idle. Nothing is running."))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
			[
				SNew(SBorder)
				.Visibility(this, &SACFLocalizationPanel::GetErrorVisibility)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.55f, 0.08f, 0.08f, 1.0f))
				.Padding(8.0f)
				[
					SAssignNew(ErrorText, STextBlock)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.75f)))
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0, 8)
			[
				SAssignNew(LogText, SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.AutoWrapText(true)
				.AlwaysShowScrollbars(true)
				.Text(LOCTEXT("LogEmpty", "Activity log appears here while a translation is running."))
			]
		]
	];

	if (UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		StatusHandle = Subsystem->OnStatusChanged().AddLambda([this](const FString&)
		{
			RefreshJobView();
		});
		FinishedHandle = Subsystem->OnJobFinished().AddLambda([this]()
		{
			RefreshJobView();
			RefreshAvailability();
		});
	}
	RefreshAvailability();
	RefreshGatherPaths();
}

SACFLocalizationPanel::~SACFLocalizationPanel()
{
	if (UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->OnStatusChanged().Remove(StatusHandle);
		Subsystem->OnJobFinished().Remove(FinishedHandle);
	}
}

UACFLocalizationSubsystem* SACFLocalizationPanel::GetSubsystem() const
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UACFLocalizationSubsystem>();
	}
	return nullptr;
}

void SACFLocalizationPanel::RefreshTargets()
{
	TargetNames.Reset();
	SelectedTargetName.Reset();
	if (UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		for (ULocalizationTarget* Target : Subsystem->GetGameTargets())
		{
			if (IsValid(Target) && !Target->Settings.Name.IsEmpty())
			{
				TargetNames.Add(MakeShared<FString>(Target->Settings.Name));
			}
		}
	}
	if (TargetNames.Num() > 0)
	{
		SelectedTargetName = TargetNames[0];
	}
	if (TargetCombo.IsValid())
	{
		TargetCombo->RefreshOptions();
		TargetCombo->SetSelectedItem(SelectedTargetName);
	}
	RefreshGatherPaths();
}

void SACFLocalizationPanel::RefreshGatherPaths()
{
	GatherPaths.Reset();
	SelectedGatherPath.Reset();
	if (UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		for (const FString& Path : Subsystem->GetGatherIncludePaths(GetSelectedTarget()))
		{
			GatherPaths.Add(MakeShared<FString>(Path));
		}
	}
	if (GatherPathList.IsValid())
	{
		GatherPathList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SACFLocalizationPanel::OnGenerateGatherPathRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()))
		];
}

FReply SACFLocalizationPanel::OnAddGatherPath()
{
	if (UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		const FString Folder = GatherPathTextBox.IsValid() ? GatherPathTextBox->GetText().ToString() : FString();
		if (Subsystem->AddGatherIncludePath(GetSelectedTarget(), Folder) && GatherPathTextBox.IsValid())
		{
			GatherPathTextBox->SetText(FText::GetEmpty());
		}
	}
	RefreshGatherPaths();
	return FReply::Handled();
}

FReply SACFLocalizationPanel::OnRemoveGatherPath()
{
	if (SelectedGatherPath.IsValid())
	{
		if (UACFLocalizationSubsystem* Subsystem = GetSubsystem())
		{
			Subsystem->RemoveGatherIncludePath(GetSelectedTarget(), *SelectedGatherPath);
		}
	}
	RefreshGatherPaths();
	return FReply::Handled();
}

ULocalizationTarget* SACFLocalizationPanel::GetSelectedTarget() const
{
	if (!SelectedTargetName.IsValid())
	{
		return nullptr;
	}
	if (UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		for (ULocalizationTarget* Target : Subsystem->GetGameTargets())
		{
			if (IsValid(Target) && Target->Settings.Name == *SelectedTargetName)
			{
				return Target;
			}
		}
	}
	return nullptr;
}

void SACFLocalizationPanel::RefreshAvailability()
{
	FString Reason;
	const bool bAvailable = FACFAssistantCompletion::IsAvailable(&Reason);
	if (AvailabilityText.IsValid())
	{
		AvailabilityText->SetText(FText::FromString(Reason.IsEmpty()
			? TEXT("Open Yarin and sign in with ChatGPT or Claude first.")
			: Reason));
		AvailabilityText->SetColorAndOpacity(bAvailable
			? FSlateColor(FLinearColor(0.2f, 0.8f, 0.3f))
			: FSlateColor(FLinearColor(0.9f, 0.5f, 0.2f)));
	}
}

FReply SACFLocalizationPanel::OnRefreshClicked()
{
	RefreshTargets();
	RefreshAvailability();
	return FReply::Handled();
}

void SACFLocalizationPanel::OnTranslateClicked()
{
	UACFLocalizationSubsystem* Subsystem = GetSubsystem();
	ULocalizationTarget* Target = GetSelectedTarget();
	if (!Subsystem || !Target || !CultureTextBox.IsValid())
	{
		return;
	}
	const bool bOverwrite = OverwriteCheckBox.IsValid() && OverwriteCheckBox->IsChecked();
	Subsystem->StartTranslation(Target, CultureTextBox->GetText().ToString(), bOverwrite);
}

void SACFLocalizationPanel::OnCancelClicked()
{
	if (UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->Cancel();
	}
}

void SACFLocalizationPanel::RefreshJobView()
{
	const UACFLocalizationSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(Subsystem->GetStatus().IsEmpty()
			? TEXT("Idle. Nothing is running.")
			: Subsystem->GetStatus()));
		const bool bFailed = Subsystem->GetStatus().StartsWith(TEXT("FAILED"));
		const bool bDone = Subsystem->GetStatus().StartsWith(TEXT("DONE"));
		StatusText->SetColorAndOpacity(bFailed
			? FSlateColor(FLinearColor(1.0f, 0.35f, 0.35f))
			: bDone
				? FSlateColor(FLinearColor(0.35f, 0.9f, 0.4f))
				: FSlateColor::UseForeground());
	}
	if (ErrorText.IsValid())
	{
		ErrorText->SetText(FText::FromString(Subsystem->GetLastError()));
	}
	if (WorkingText.IsValid())
	{
		const FString& LiveStatus = Subsystem->GetStatus();
		if (!Subsystem->IsBusy())
		{
			WorkingText->SetText(FText::GetEmpty());
		}
		else if (LiveStatus.Contains(TEXT("Step 1 of 3")))
		{
			WorkingText->SetText(LOCTEXT("WorkingSearch", "WORKING — searching texts..."));
		}
		else if (LiveStatus.Contains(TEXT("Step 2 of 3")))
		{
			WorkingText->SetText(LOCTEXT("WorkingExport", "WORKING — writing the text file..."));
		}
		else
		{
			WorkingText->SetText(LOCTEXT("WorkingTranslate", "WORKING — translating..."));
		}
	}
	if (ProgressBar.IsValid())
	{
		const int32 Total = Subsystem->GetProgressTotal();
		if (Subsystem->IsBusy() && Total <= 0)
		{
			ProgressBar->SetPercent(TOptional<float>());
		}
		else
		{
			const float Percent = Total > 0
				? static_cast<float>(Subsystem->GetProgressDone()) / static_cast<float>(Total)
				: 0.0f;
			ProgressBar->SetPercent(Percent);
		}
	}
	if (LogText.IsValid())
	{
		LogText->SetText(GetLogText());
		LogText->ScrollTo(ETextLocation::EndOfDocument);
	}
}

FText SACFLocalizationPanel::GetLogText() const
{
	const UACFLocalizationSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || Subsystem->GetLogLines().Num() == 0)
	{
		return LOCTEXT("LogEmpty", "Activity log appears here while a translation is running.");
	}
	return FText::FromString(FString::Join(Subsystem->GetLogLines(), TEXT("\n")));
}

EVisibility SACFLocalizationPanel::GetBusyVisibility() const
{
	if (const UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		return Subsystem->IsBusy() ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

EVisibility SACFLocalizationPanel::GetErrorVisibility() const
{
	if (const UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		return Subsystem->HasError() ? EVisibility::Visible : EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

bool SACFLocalizationPanel::IsTranslateEnabled() const
{
	if (const UACFLocalizationSubsystem* Subsystem = GetSubsystem())
	{
		return !Subsystem->IsBusy() && GetSelectedTarget() != nullptr;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
