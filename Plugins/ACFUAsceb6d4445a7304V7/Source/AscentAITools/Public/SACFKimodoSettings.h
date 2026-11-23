// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;

/**
 * Kimodo rows in the Yarin settings window. Two separate ways in: Use Existing points at a Kimodo folder that is already
 * installed and never downloads; Download asks where to install, confirms, and installs. The log opens in its own window.
 */
class ASCENTAITOOLS_API SACFKimodoSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFKimodoSettings) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	class UACFKimodoSubsystem* Kimodo() const;
	class UACFAIToolsSettings* Settings() const;
	TSharedRef<SWidget> MakeRow(const FString& Label, const TSharedRef<SWidget>& Content) const;
	TSharedRef<SWidget> MakeButton(const FString& Label, const FString& ToolTip, FReply (SACFKimodoSettings::*Handler)(), EVisibility (SACFKimodoSettings::*Shown)() const);
	FText StatusText() const;
	FSlateColor StatusColor() const;
	FText FolderText() const;
	FText ExecutableText() const;
	EVisibility ShowWhenIdle() const;
	EVisibility ShowWhenBusy() const;
	EVisibility ShowWhenReady() const;
	FReply OnUseExisting();
	FReply OnDownload();
	FReply OnCancel();
	FReply OnShowLog();
	bool PickFolder(const FString& Title, FString& OutFolder) const;
	void SetFolder(const FString& Folder);
	void OpenLogWindow();
	void CommitModel(const FText& Text, ETextCommit::Type CommitType);
	void CommitTextEncoder(ECheckBoxState State);

	TWeakPtr<SWindow> LogWindow;
};
