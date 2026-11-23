// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SACFKimodoSettings.h"

#include "ACFKimodoSubsystem.h"
#include "AscentAIToolsSettings.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

void SACFKimodoSettings::Construct(const FArguments& InArgs)
{
	const FSlateColor Dim(FLinearColor(0.55f, 0.58f, 0.64f));
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Motion (Kimodo)")))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text(this, &SACFKimodoSettings::StatusText)
					.ToolTipText(this, &SACFKimodoSettings::StatusText)
					.ColorAndOpacity(this, &SACFKimodoSettings::StatusColor)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeButton(TEXT("Use existing"), TEXT("You already have Kimodo: select its folder (the one with venv, hf and text-encoders). Nothing is downloaded."),
						&SACFKimodoSettings::OnUseExisting, &SACFKimodoSettings::ShowWhenIdle)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeButton(TEXT("Download"), TEXT("You do not have Kimodo: choose where to install it (about 25 GB). You confirm before anything is downloaded."),
						&SACFKimodoSettings::OnDownload, &SACFKimodoSettings::ShowWhenIdle)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeButton(TEXT("Change folder"), TEXT("Use a different Kimodo folder that is already installed."),
						&SACFKimodoSettings::OnUseExisting, &SACFKimodoSettings::ShowWhenReady)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					MakeButton(TEXT("Cancel"), TEXT("Stops the setup or generation that is running."),
						&SACFKimodoSettings::OnCancel, &SACFKimodoSettings::ShowWhenBusy)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Log")))
					.ToolTipText(FText::FromString(TEXT("Opens the Kimodo log in its own window.")))
					.OnClicked(this, &SACFKimodoSettings::OnShowLog)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeRow(TEXT("Folder"),
					SNew(STextBlock)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text(this, &SACFKimodoSettings::FolderText)
					.ToolTipText(this, &SACFKimodoSettings::FolderText))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeRow(TEXT("Program"),
					SNew(STextBlock)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.Text(this, &SACFKimodoSettings::ExecutableText)
					.ToolTipText(this, &SACFKimodoSettings::ExecutableText)
					.ColorAndOpacity(Dim))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeRow(TEXT("Model"),
					SNew(SEditableTextBox)
					.Text_Lambda([this]()
					{
						const UACFAIToolsSettings* Current = Settings();
						return FText::FromString(Current ? Current->KimodoModel : FString());
					})
					.OnTextCommitted(this, &SACFKimodoSettings::CommitModel))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SCheckBox)
				.ToolTipText(FText::FromString(TEXT("Under 3 GB of video memory, slower.")))
				.IsChecked_Lambda([this]()
				{
					const UACFAIToolsSettings* Current = Settings();
					return Current && Current->bKimodoTextEncoderOnCpu ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged(this, &SACFKimodoSettings::CommitTextEncoder)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Text encoder on CPU")))
				]
			]
		]
	];
}

TSharedRef<SWidget> SACFKimodoSettings::MakeRow(const FString& Label, const TSharedRef<SWidget>& Content) const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(90.0f)
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			Content
		];
}

TSharedRef<SWidget> SACFKimodoSettings::MakeButton(const FString& Label, const FString& ToolTip, FReply (SACFKimodoSettings::*Handler)(), EVisibility (SACFKimodoSettings::*Shown)() const)
{
	return SNew(SBox)
		.Visibility(this, Shown)
		.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
		[
			SNew(SButton)
			.Text(FText::FromString(Label))
			.ToolTipText(FText::FromString(ToolTip))
			.OnClicked(this, Handler)
		];
}

UACFKimodoSubsystem* SACFKimodoSettings::Kimodo() const
{
	return GEditor ? GEditor->GetEditorSubsystem<UACFKimodoSubsystem>() : nullptr;
}

UACFAIToolsSettings* SACFKimodoSettings::Settings() const
{
	return GetMutableDefault<UACFAIToolsSettings>();
}

FText SACFKimodoSettings::StatusText() const
{
	const UACFKimodoSubsystem* Runtime = Kimodo();
	return Runtime ? Runtime->GetStatus() : FText::FromString(TEXT("Unavailable"));
}

FSlateColor SACFKimodoSettings::StatusColor() const
{
	const UACFKimodoSubsystem* Runtime = Kimodo();
	if (Runtime && Runtime->IsFailed())
	{
		return FSlateColor(FLinearColor(0.9f, 0.4f, 0.3f));
	}
	if (Runtime && Runtime->IsReady())
	{
		return FSlateColor(FLinearColor(0.35f, 0.8f, 0.35f));
	}
	return FSlateColor(FLinearColor(0.55f, 0.58f, 0.64f));
}

FText SACFKimodoSettings::FolderText() const
{
	const UACFKimodoSubsystem* Runtime = Kimodo();
	const FString Folder = Runtime ? Runtime->GetInstallFolder() : FString();
	return FText::FromString(Folder.IsEmpty() ? TEXT("None. Use existing if you have Kimodo, Download if you do not.") : Folder);
}

FText SACFKimodoSettings::ExecutableText() const
{
	const UACFKimodoSubsystem* Runtime = Kimodo();
	const FString Found = Runtime ? Runtime->GetExecutable() : FString();
	return FText::FromString(Found.IsEmpty() ? TEXT("Not found") : Found);
}

EVisibility SACFKimodoSettings::ShowWhenIdle() const
{
	const UACFKimodoSubsystem* Runtime = Kimodo();
	return Runtime && !Runtime->IsBusy() && !Runtime->IsReady() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SACFKimodoSettings::ShowWhenBusy() const
{
	const UACFKimodoSubsystem* Runtime = Kimodo();
	return Runtime && Runtime->IsBusy() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SACFKimodoSettings::ShowWhenReady() const
{
	const UACFKimodoSubsystem* Runtime = Kimodo();
	return Runtime && Runtime->IsReady() && !Runtime->IsBusy() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SACFKimodoSettings::PickFolder(const FString& Title, FString& OutFolder) const
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		return false;
	}
	const UACFKimodoSubsystem* Runtime = Kimodo();
	const FString Current = Runtime ? Runtime->GetInstallFolder() : FString();
	const FString Start = Runtime && Runtime->IsKimodoFolder(Current) ? FPaths::GetPath(Current) : FString(FPlatformProcess::UserDir());
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	return Desktop->OpenDirectoryDialog(Parent, Title, Start, OutFolder) && !OutFolder.IsEmpty();
}

void SACFKimodoSettings::SetFolder(const FString& Folder)
{
	if (UACFAIToolsSettings* Current = Settings())
	{
		Current->KimodoInstallFolder = Folder;
		Current->SaveConfig();
	}
	if (UACFKimodoSubsystem* Runtime = Kimodo())
	{
		Runtime->Refresh();
	}
}

FReply SACFKimodoSettings::OnUseExisting()
{
	UACFKimodoSubsystem* Runtime = Kimodo();
	FString Picked;
	if (!Runtime || Runtime->IsBusy() || !PickFolder(TEXT("Select your Kimodo folder (the one with venv, hf and text-encoders)"), Picked))
	{
		return FReply::Handled();
	}
	const FString Folder = Runtime->GetExistingFolderForPick(Picked);
	if (!Runtime->IsKimodoFolder(Folder))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(
			TEXT("There is no Kimodo in:\n%s\n\nSelect the folder that contains venv, hf and text-encoders.\nTo install Kimodo, press Download instead."), *FPaths::ConvertRelativePathToFull(Picked))),
			FText::FromString(TEXT("Use existing Kimodo")));
		return FReply::Handled();
	}
	SetFolder(Folder);
	return FReply::Handled();
}

FReply SACFKimodoSettings::OnDownload()
{
	UACFKimodoSubsystem* Runtime = Kimodo();
	if (!Runtime || Runtime->IsBusy())
	{
		return FReply::Handled();
	}
	const FString Previous = Runtime->GetInstallFolder();
	// A folder with an unfinished install is completed in place; otherwise the user picks where the new install goes.
	if (Previous.IsEmpty() || !FPaths::DirectoryExists(Previous / TEXT("venv")))
	{
		FString Picked;
		if (!PickFolder(TEXT("Where should Kimodo be installed? (about 25 GB)"), Picked))
		{
			return FReply::Handled();
		}
		SetFolder(Runtime->GetFolderForPick(Picked));
	}
	if (!Runtime->NeedsDownload())
	{
		Runtime->Refresh();
		return FReply::Handled();
	}
	if (FMessageDialog::Open(EAppMsgType::OkCancel, Runtime->GetDownloadSummary(), FText::FromString(TEXT("Download Kimodo"))) != EAppReturnType::Ok)
	{
		SetFolder(Previous);
		return FReply::Handled();
	}
	Runtime->Download();
	if (Runtime->IsBusy() || Runtime->IsFailed())
	{
		OpenLogWindow();
	}
	return FReply::Handled();
}

FReply SACFKimodoSettings::OnCancel()
{
	if (UACFKimodoSubsystem* Runtime = Kimodo())
	{
		Runtime->Cancel();
	}
	return FReply::Handled();
}

FReply SACFKimodoSettings::OnShowLog()
{
	OpenLogWindow();
	return FReply::Handled();
}

void SACFKimodoSettings::OpenLogWindow()
{
	if (const TSharedPtr<SWindow> Existing = LogWindow.Pin())
	{
		Existing->BringToFront();
		return;
	}
	TWeakObjectPtr<UACFKimodoSubsystem> Runtime(Kimodo());
	const TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("Kimodo log")))
		.ClientSize(FVector2D(820.0f, 480.0f))
		.SupportsMinimize(true)
		.SupportsMaximize(true)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.0f)
			[
				SNew(SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.AlwaysShowScrollbars(true)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), 9))
				.Text_Lambda([Runtime]()
				{
					const UACFKimodoSubsystem* Live = Runtime.Get();
					return FText::FromString(Live ? Live->GetLog() : FString());
				})
			]
		];
	FSlateApplication::Get().AddWindow(Window);
	LogWindow = Window;
}

void SACFKimodoSettings::CommitModel(const FText& Text, ETextCommit::Type CommitType)
{
	if (UACFAIToolsSettings* Current = Settings())
	{
		Current->KimodoModel = Text.ToString().TrimStartAndEnd();
		if (Current->KimodoModel.IsEmpty())
		{
			Current->KimodoModel = TEXT("Kimodo-SOMA-RP-v1.2");
		}
		Current->SaveConfig();
	}
}

void SACFKimodoSettings::CommitTextEncoder(ECheckBoxState State)
{
	if (UACFAIToolsSettings* Current = Settings())
	{
		Current->bKimodoTextEncoderOnCpu = State == ECheckBoxState::Checked;
		Current->SaveConfig();
	}
}
