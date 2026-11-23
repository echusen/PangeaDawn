// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SACFAssistantMeshCard.h"

#include "AscentAIToolsSubsystem.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "SACFMeshPreview.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ACFAssistantMeshCard"

FText SACFAssistantMeshCard::PendingStatus() const
{
	// The subsystem only has one mesh job at a time, so its latest status belongs to this card. Once the service has been seen
	// working, its latest line (download, import) stays up until the tool answers; before that nothing has started yet.
	const UACFAIToolsSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UACFAIToolsSubsystem>() : nullptr;
	if (bSawBusy && Subsystem && !Subsystem->GetLastStatus().IsEmpty())
	{
		return Subsystem->GetLastStatus();
	}
	return LOCTEXT("Starting", "Starting the 3D generation...");
}

void SACFAssistantMeshCard::Construct(const FArguments& InArgs)
{
	OnGetState = InArgs._OnGetState;
	AccentColor = InArgs._AccentColor;
	MutedColor = InArgs._MutedColor;
	LabelFont = InArgs._LabelFont;
	OnSizeChanged = InArgs._OnSizeChanged;

	ChildSlot
	[
		SAssignNew(Body, SBox)
	];
	Rebuild(OnGetState.IsBound() ? OnGetState.Execute() : FACFMeshCardState());
}

void SACFAssistantMeshCard::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// The layout of the previous frame already holds the new size of a rebuilt card, so this is the earliest honest moment to say it changed.
	if (bNotifySizeNextTick && bBuilt)
	{
		bNotifySizeNextTick = false;
		OnSizeChanged.ExecuteIfBound();
	}
	if (!bSawBusy)
	{
		const UACFAIToolsSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UACFAIToolsSubsystem>() : nullptr;
		bSawBusy = Subsystem && Subsystem->IsMeshBusy();
	}
	if (!OnGetState.IsBound()) { return; }
	const FACFMeshCardState State = OnGetState.Execute();
	if (!bBuilt || !(State == Built)) { Rebuild(State); }
}

void SACFAssistantMeshCard::Rebuild(const FACFMeshCardState& State)
{
	Built = State;
	bBuilt = true;
	bNotifySizeNextTick = true;
	switch (State.Kind)
	{
	case EACFMeshCardKind::Pending: Body->SetContent(MakePending()); break;
	case EACFMeshCardKind::Ready: Body->SetContent(MakeReady(State.Text)); break;
	case EACFMeshCardKind::Failed: Body->SetContent(MakeFailed(State.Text)); break;
	}
}

TSharedRef<SWidget> SACFAssistantMeshCard::MakePending() const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
		[
			SNew(SCircularThrobber).Radius(10.0f)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(LabelFont)
				.ColorAndOpacity(AccentColor)
				.Text(LOCTEXT("Generating", "Generating a 3D mesh"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(MutedColor)
				.Text_Lambda([this] { return PendingStatus(); })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
				.ColorAndOpacity(MutedColor)
				.Text(LOCTEXT("PendingHint", "This takes a few minutes. Yarin continues when the mesh is ready."))
			]
		];
}

TSharedRef<SWidget> SACFAssistantMeshCard::MakeReady(const FString& ObjectPath) const
{
	const FString Name = FPaths::GetBaseFilename(ObjectPath);
	UObject* Mesh = ACFMeshPreview::LoadMesh(ObjectPath);
	if (!Mesh)
	{
		return SNew(STextBlock)
			.AutoWrapText(true)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
			.ColorAndOpacity(MutedColor)
			.Text(FText::Format(LOCTEXT("MeshGone", "The mesh {0} is no longer in the project."), FText::FromString(Name)));
	}
	const TWeakObjectPtr<UObject> WeakMesh(Mesh);
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Font(LabelFont)
			.ColorAndOpacity(AccentColor)
			.Text(FText::Format(LOCTEXT("MeshReady", "3D mesh: {0}"), FText::FromString(Name)))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
		[
			SNew(SACFMeshPreview, Mesh)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("OpenTip", "Open the mesh in its own Unreal editor"))
				.OnClicked_Lambda([WeakMesh]
				{
					if (UObject* Live = WeakMesh.Get()) { ACFMeshPreview::OpenInEditor(Live); }
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("Open", "Open"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("BrowseTip", "Select the mesh in the Content Browser"))
				.OnClicked_Lambda([WeakMesh]
				{
					if (UObject* Live = WeakMesh.Get()) { ACFMeshPreview::ShowInContentBrowser(Live); }
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("Browse", "Show in Content Browser"))
				]
			]
		];
}

TSharedRef<SWidget> SACFAssistantMeshCard::MakeFailed(const FString& Reason) const
{
	return SNew(STextBlock)
		.AutoWrapText(true)
		.ColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.45f))
		.Text(FText::Format(LOCTEXT("MeshFailed", "3D generation failed: {0}"), FText::FromString(Reason)));
}

#undef LOCTEXT_NAMESPACE
