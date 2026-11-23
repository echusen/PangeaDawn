// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"

enum class EACFMeshCardKind : uint8
{
	Pending,
	Ready,
	Failed
};

struct FACFMeshCardState
{
	EACFMeshCardKind Kind = EACFMeshCardKind::Pending;
	/** The object path of the mesh when Ready, the reason when Failed. */
	FString Text;

	bool operator==(const FACFMeshCardState& Other) const { return Kind == Other.Kind && Text == Other.Text; }
};

/**
 * One 3D generation in the chat thread. It follows the state of its message: a live progress card while the cloud service
 * works, the 3D viewer with Open and Show in Content Browser once the mesh is imported, or the reason it failed. The card
 * rebuilds itself when the state changes, so the thread never has to be rebuilt for it.
 */
class SACFAssistantMeshCard : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(FACFMeshCardState, FOnGetMeshCardState);

	SLATE_BEGIN_ARGS(SACFAssistantMeshCard)
		: _AccentColor(FLinearColor::White)
		, _MutedColor(FLinearColor(0.6f, 0.6f, 0.6f))
	{}
		/** Reads the current state of the message behind this card. Called every frame the card is visible, so it must be cheap. */
		SLATE_EVENT(FOnGetMeshCardState, OnGetState)
		/** Called one frame after the card replaced its content, when its height has changed. */
		SLATE_EVENT(FSimpleDelegate, OnSizeChanged)
		SLATE_ARGUMENT(FSlateColor, AccentColor)
		SLATE_ARGUMENT(FSlateColor, MutedColor)
		SLATE_ARGUMENT(FSlateFontInfo, LabelFont)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void Rebuild(const FACFMeshCardState& State);
	FText PendingStatus() const;
	TSharedRef<SWidget> MakePending() const;
	TSharedRef<SWidget> MakeReady(const FString& ObjectPath) const;
	TSharedRef<SWidget> MakeFailed(const FString& Reason) const;

	FOnGetMeshCardState OnGetState;
	FSlateColor AccentColor;
	FSlateColor MutedColor;
	FSlateFontInfo LabelFont;
	FACFMeshCardState Built;
	bool bBuilt = false;
	/** True once the cloud service has been seen working for this card; from then on the live status line is shown. */
	bool bSawBusy = false;
	/** Set when the content was just replaced; the next Tick reports it so the owner can re-scroll. */
	bool bNotifySizeNextTick = false;
	FSimpleDelegate OnSizeChanged;
	TSharedPtr<SBox> Body;
};
