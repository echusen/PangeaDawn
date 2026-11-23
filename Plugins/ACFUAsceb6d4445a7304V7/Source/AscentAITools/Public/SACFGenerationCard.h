// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenerationSections.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;

/**
 * One generation feature as a card: the section's title, a two option selector with "Yarin credit" on the left
 * (a RECOMMENDED badge when it is the effective default) and "My own key" on the right, then the body of the
 * chosen side. Every state decision comes from ComputeGenerationCardState, so the widget only renders.
 * This version always renders the own API key body. The selector is kept in the .cpp under #if 0.
 *
 * The card refreshes itself when the catalog snapshot changes; it subscribes in Construct and unsubscribes on
 * destruction, so it is safe to build and discard freely.
 */
class ASCENTAITOOLS_API SACFGenerationCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFGenerationCard)
		: _ShowBalance(true)
	{}
		/** The section this card renders. */
		SLATE_ARGUMENT(TSharedPtr<IACFGenerationSection>, Section)
		/** The remaining credit belongs to the account, not to one feature, so a list of cards shows it on one card only. */
		SLATE_ARGUMENT(bool, ShowBalance)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SACFGenerationCard() override;

private:
	/** The card's rules for the section and the current snapshot. */
	FACFGenerationCardState CardState() const;

	/** Rebuilds the whole card from the current snapshot and the section's settings. */
	void RebuildContent();

	/** One tile of the two option selector. */
	TSharedRef<SWidget> MakeOptionTile(const FText& Label, bool bChosen, bool bLocked, bool bRecommended,
		const FString& SubText, EACFGenerationSource Source);
	TSharedRef<SWidget> MakeBalanceRow();
	TSharedRef<SWidget> MakeCatalogNotes(bool bShowLoading, bool bShowRetry);
	TSharedRef<SWidget> MakeFallbackBanner(const FString& Reason);

	FReply OnOptionClicked(EACFGenerationSource Source);
	FReply OnAddCreditClicked();
	FReply OnUseMyKeyClicked();
	FReply OnRetryClicked();

	/** The section is never null (SACFGenerationSettings passes registered sections). A shared ref member would default construct the abstract interface. */
	TSharedPtr<IACFGenerationSection> Section;
	/** False on every card but the one the list chose to carry the balance line. */
	bool bShowBalance = true;
	FDelegateHandle CatalogChangedHandle;
	/** The card's body: cleared and refilled on every rebuild, inside a fixed border. */
	TSharedPtr<SVerticalBox> ContentBox;
};
