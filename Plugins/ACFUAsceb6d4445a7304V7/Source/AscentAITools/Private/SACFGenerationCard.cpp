// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SACFGenerationCard.h"

#include "MediaGateway.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "AscentAITools"

namespace
{
	constexpr float GenerationCardCornerRadius = 8.0f;

	// Dusty parchment gold, the same accent the assistant panel uses, so the cards read the same in both homes.
	FLinearColor GoldLinear(const uint8 R, const uint8 G, const uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}
	FSlateColor GoldAccent() { return GoldLinear(182, 176, 154); }

	const FSlateBrush& CardBackgroundBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FAppStyle::GetBrush("ToolPanel.GroupBorder")->TintColor, GenerationCardCornerRadius);
		return Brush;
	}

	// The assistant's filled button style, rebuilt here because AscentAITools must not depend on the assistant module.
	// The outline is what makes an unchosen option read as a button instead of a label next to the gold chosen one.
	const FButtonStyle& FilledOptionStyle()
	{
		static const FButtonStyle Style = []
		{
			FButtonStyle Result = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton");
			const FLinearColor Outline = FStyleColors::InputOutline.GetSpecifiedColor();
			const FLinearColor DimmedOutline(Outline.R, Outline.G, Outline.B, 0.4f);
			Result.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, GenerationCardCornerRadius, Outline, 1.0f))
				.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Dropdown.GetSpecifiedColor(), GenerationCardCornerRadius, Outline, 1.0f))
				.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Input.GetSpecifiedColor(), GenerationCardCornerRadius, Outline, 1.0f))
				.SetDisabled(FSlateRoundedBoxBrush(FLinearColor::Transparent, GenerationCardCornerRadius, DimmedOutline, 1.0f));
			return Result;
		}();
		return Style;
	}

	// The chosen option: the same gold pill the assistant uses for its primary actions.
	const FButtonStyle& ChosenOptionStyle()
	{
		static const FButtonStyle Style = []
		{
			FButtonStyle Result = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton");
			const FLinearColor Edge = GoldLinear(0xA4, 0x8F, 0x52);
			Result.SetNormal(FSlateRoundedBoxBrush(GoldLinear(0xC2, 0xAD, 0x6C), GenerationCardCornerRadius, Edge, 1.0f))
				.SetHovered(FSlateRoundedBoxBrush(GoldLinear(0xD6, 0xC4, 0x8C), GenerationCardCornerRadius, Edge, 1.0f))
				.SetPressed(FSlateRoundedBoxBrush(GoldLinear(0xB4, 0xA0, 0x61), GenerationCardCornerRadius, Edge, 1.0f))
				.SetDisabled(FSlateRoundedBoxBrush(GoldLinear(0x7A, 0x70, 0x52), GenerationCardCornerRadius, Edge, 1.0f));
			return Result;
		}();
		return Style;
	}

	const FSlateBrush& BadgeBrush()
	{
		static const FSlateRoundedBoxBrush Brush(GoldLinear(0xC2, 0xAD, 0x6C), 4.0f);
		return Brush;
	}

	FSlateColor ChosenText() { return GoldLinear(0x2A, 0x24, 0x16); }
	FSlateColor UnchosenText() { return FSlateColor::UseForeground(); }
	FSlateColor DimmedText() { return FLinearColor(1.0f, 1.0f, 1.0f, 0.45f); }
	FSlateColor MutedText() { return FLinearColor(0.55f, 0.58f, 0.64f); }
	FSlateColor WarningText() { return FLinearColor(0.95f, 0.75f, 0.35f); }

	FSlateColor TileTextColor(const bool bChosen, const bool bLocked)
	{
		return bLocked ? DimmedText() : (bChosen ? ChosenText() : UnchosenText());
	}
}

void SACFGenerationCard::Construct(const FArguments& InArgs)
{
	Section = InArgs._Section;
	bShowBalance = InArgs._ShowBalance;
	CatalogChangedHandle = FACFMediaCatalog::OnChanged().AddSP(this, &SACFGenerationCard::RebuildContent);

	// The frame is fixed; RebuildContent clears and refills the body below it.
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&CardBackgroundBrush())
		.Padding(10.0f)
		[
			SAssignNew(ContentBox, SVerticalBox)
		]
	];
	RebuildContent();
}

SACFGenerationCard::~SACFGenerationCard()
{
	FACFMediaCatalog::OnChanged().Remove(CatalogChangedHandle);
}

FACFGenerationCardState SACFGenerationCard::CardState() const
{
	const TSharedRef<const FACFMediaCatalogSnapshot> SnapshotHandle = FACFMediaCatalog::GetSnapshot();
	const FACFMediaCatalogSnapshot& Snapshot = *SnapshotHandle;
	return ComputeGenerationCardState(Section->ReadEntitlement(Snapshot), Snapshot, FACFMediaGateway::IsPresent(),
		Section->GetSetting(), Section->HasOwnKey());
}

void SACFGenerationCard::RebuildContent()
{
	if (!ContentBox.IsValid())
	{
		return;
	}
	const FACFGenerationCardState State = CardState();

	TSharedRef<SVerticalBox> Content = ContentBox.ToSharedRef();
	Content->ClearChildren();

	// Title.
	Content->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
	[
		SNew(STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
		.ColorAndOpacity(GoldAccent())
		.Text(Section->GetTitle())
	];

	// The old selector is kept, not removed. This version does not show a replacement label: only the API key fields.
#if 0
	const TSharedRef<const FACFMediaCatalogSnapshot> SnapshotHandle = FACFMediaCatalog::GetSnapshot();
	const FACFMediaCatalogSnapshot& Snapshot = *SnapshotHandle;

	// The two option selector: Yarin credit on the left, own key on the right.
	Content->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
		[
			MakeOptionTile(LOCTEXT("YarinOption", "Yarin credit"), State.EffectiveSource == EACFGenerationSource::Yarin,
				State.bYarinLocked, State.bYarinRecommended, State.LockReason, EACFGenerationSource::Yarin)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4, 0, 0, 0)
		[
			MakeOptionTile(LOCTEXT("OwnKeyOption", "My own key"), State.EffectiveSource == EACFGenerationSource::OwnKey,
				/*bLocked*/ false, /*bRecommended*/ false, FString(), EACFGenerationSource::OwnKey)
		]
	];
#endif
#if 0
	// Coming Soon was shown here in place of the selector. Removed from this version: it did not say where to go.
	Content->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
	[
		SNew(SButton)
		.ButtonStyle(&FilledOptionStyle())
		.IsEnabled(false)
		.Cursor(EMouseCursor::Default)
		.ToolTipText(LOCTEXT("YarinCreditComingSoonTip", "Yarin credit is coming in a later version. This version uses your own API key."))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
				.ColorAndOpacity(DimmedText())
				.Text(LOCTEXT("YarinCreditComingSoon", "Coming Soon"))
			]
		]
	];
#endif

	if (State.bShowBalance && bShowBalance)
	{
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			MakeBalanceRow()
		];
	}

	if (State.bShowLoadingNote || State.bShowRetryLink)
	{
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 4)
		[
			MakeCatalogNotes(State.bShowLoadingNote, State.bShowRetryLink)
		];
	}

	// The chosen body, kept for when the selector above is turned back on.
#if 0
	// Rule (e): the chosen Yarin source cannot serve, so the card says why and offers both ways out.
	if (State.bShowFallbackMessage)
	{
		Content->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
		[
			MakeFallbackBanner(State.FallbackReason)
		];
	}

	// The chosen body. A locked Yarin option shows no body: the lock reason and the connect invite say it all.
	if (State.EffectiveSource == EACFGenerationSource::Yarin)
	{
		if (State.bYarinEnabled)
		{
			Content->AddSlot().AutoHeight()
			[
				Section->BuildYarinBody(Snapshot)
			];
		}
	}
	else
#endif
	{
		Content->AddSlot().AutoHeight()
		[
			Section->BuildOwnKeyBody()
		];
	}
}

TSharedRef<SWidget> SACFGenerationCard::MakeOptionTile(const FText& Label, const bool bChosen, const bool bLocked,
	const bool bRecommended, const FString& SubText, const EACFGenerationSource Source)
{
	TSharedRef<SVerticalBox> Tile = SNew(SVerticalBox);

	// Label row: padlock glyph when locked, a check when chosen, the label, then the RECOMMENDED badge.
	Tile->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
		[
			SNew(SBox).Visibility(bLocked ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Icons.Lock")))
			]
		]
		// The check says "this one is selected", so the gold tile is never mistaken for an action.
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
		[
			SNew(SBox).Visibility(bChosen && !bLocked ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(TEXT("Icons.Check")))
				.ColorAndOpacity(ChosenText())
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 10))
			.ColorAndOpacity(TileTextColor(bChosen, bLocked))
			.Text(Label)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
		[
			SNew(SBorder)
			.Visibility(bRecommended ? EVisibility::Visible : EVisibility::Collapsed)
			.BorderImage(&BadgeBrush())
			.Padding(FMargin(5, 1))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 7))
				.ColorAndOpacity(ChosenText())
				.Text(LOCTEXT("RecommendedBadge", "RECOMMENDED"))
			]
		]
	];

	// The lock reason under a locked option ("Included from Hobbyist", "Sign in to Yarin").
	if (bLocked && !SubText.IsEmpty())
	{
		Tile->AddSlot().AutoHeight().Padding(14, 2, 0, 0)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
			.ColorAndOpacity(DimmedText())
			.Text(FText::FromString(SubText))
		];
	}

	return SNew(SButton)
		.ButtonStyle(bChosen ? &ChosenOptionStyle() : &FilledOptionStyle())
		.IsEnabled(!bLocked)
		.Cursor(bLocked || bChosen ? EMouseCursor::Default : EMouseCursor::Hand)
		.ToolTipText(bLocked ? FText::FromString(SubText)
			: (bChosen ? LOCTEXT("OptionChosenTip", "Selected. This is what pays for this feature.")
				: LOCTEXT("OptionPickTip", "Click to choose this option.")))
		.OnClicked(this, &SACFGenerationCard::OnOptionClicked, Source)
		[
			Tile
		];
}

TSharedRef<SWidget> SACFGenerationCard::MakeBalanceRow()
{
	const TSharedRef<const FACFMediaCatalogSnapshot> SnapshotHandle = FACFMediaCatalog::GetSnapshot();
	const FACFMediaCatalogSnapshot& Snapshot = *SnapshotHandle;
	const FACFGenerationCardState State = CardState();

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
			.ColorAndOpacity(MutedText())
			.Text(FText::FromString(FACFMediaCatalog::FormatCreditsLeftThisMonth(Snapshot.RemainingCredits)))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
		[
			SNew(STextBlock)
			.Visibility(State.bShowLowCreditWarning ? EVisibility::Visible : EVisibility::Collapsed)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
			.ColorAndOpacity(WarningText())
			.Text_Lambda([]
			{
				return FText::FromString(FString::Printf(TEXT("Low credit: %s"), *FACFMediaCatalog::FormatCreditsLeftThisMonth(
					FACFMediaCatalog::GetSnapshot()->RemainingCredits)));
			})
		];
}

TSharedRef<SWidget> SACFGenerationCard::MakeCatalogNotes(const bool bShowLoading, const bool bShowRetry)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility(bShowLoading ? EVisibility::Visible : EVisibility::Collapsed)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
			.ColorAndOpacity(MutedText())
			.Text(LOCTEXT("FetchingPrices", "Fetching prices..."))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Visibility(bShowRetry ? EVisibility::Visible : EVisibility::Collapsed)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
			.ColorAndOpacity(MutedText())
			.Text(FACFMediaCatalog::GetSnapshot()->ErrorText.IsEmpty()
				? LOCTEXT("PricesFailed", "Prices could not load.")
				: FText::FromString(FACFMediaCatalog::GetSnapshot()->ErrorText))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
		[
			SNew(SButton)
			.Visibility(bShowRetry ? EVisibility::Visible : EVisibility::Collapsed)
			.ButtonStyle(&FilledOptionStyle())
			.OnClicked(this, &SACFGenerationCard::OnRetryClicked)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
				.ColorAndOpacity(GoldAccent())
				.Text(LOCTEXT("RetryPrices", "Retry"))
			]
		];
}

TSharedRef<SWidget> SACFGenerationCard::MakeFallbackBanner(const FString& Reason)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
				.ColorAndOpacity(WarningText())
				.Text(FText::FromString(Reason))
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
				[
					SNew(SButton)
					.ButtonStyle(&ChosenOptionStyle())
					.OnClicked(this, &SACFGenerationCard::OnAddCreditClicked)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 9))
						.ColorAndOpacity(ChosenText())
						.Text(LOCTEXT("AddCredit", "Add credit"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&FilledOptionStyle())
					.OnClicked(this, &SACFGenerationCard::OnUseMyKeyClicked)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
						.Text(LOCTEXT("UseMyKey", "Use my key"))
					]
				]
			]
		];
}

FReply SACFGenerationCard::OnOptionClicked(const EACFGenerationSource Source)
{
	// Clicking a side writes that side explicitly; the section persists it in Project Settings.
	Section->SetSetting(Source == EACFGenerationSource::Yarin ? EACF3DKeySource::Yarin : EACF3DKeySource::OwnKey);
	RebuildContent();
	return FReply::Handled();
}

FReply SACFGenerationCard::OnAddCreditClicked()
{
	FACFGenerationSections::OnRequestOpenPlanPage().Broadcast();
	return FReply::Handled();
}

FReply SACFGenerationCard::OnUseMyKeyClicked()
{
	// The one way out of a stuck Yarin choice that never falls back silently.
	Section->SetSetting(EACF3DKeySource::OwnKey);
	RebuildContent();
	return FReply::Handled();
}

FReply SACFGenerationCard::OnRetryClicked()
{
	FACFMediaCatalog::Refresh(true);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
