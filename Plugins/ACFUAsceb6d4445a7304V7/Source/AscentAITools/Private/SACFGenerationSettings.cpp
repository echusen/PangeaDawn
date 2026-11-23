// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SACFGenerationSettings.h"

#include "GenerationSections.h"
#include "MediaCatalog.h"
#include "MediaGateway.h"
#include "SACFGenerationCard.h"

#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

void SACFGenerationSettings::Construct(const FArguments& InArgs)
{
	// A card just opened: a stale snapshot is fetched now. The cache time and the running fetch coalesce.
	FACFMediaCatalog::Refresh(false);
	SectionsChangedHandle = FACFGenerationSections::OnChanged().AddSP(this, &SACFGenerationSettings::RebuildContent);
	CatalogChangedHandle = FACFMediaCatalog::OnChanged().AddSP(this, &SACFGenerationSettings::RebuildContent);

	ChildSlot
	[
		SAssignNew(CardList, SVerticalBox)
	];
	RebuildContent();
}

SACFGenerationSettings::~SACFGenerationSettings()
{
	FACFGenerationSections::OnChanged().Remove(SectionsChangedHandle);
	FACFMediaCatalog::OnChanged().Remove(CatalogChangedHandle);
}

void SACFGenerationSettings::RebuildContent()
{
	if (!CardList.IsValid())
	{
		return;
	}
	CardList->ClearChildren();
	// The remaining credit is the account's, so it is read once: the first card that would show it carries the line.
	const TSharedRef<const FACFMediaCatalogSnapshot> SnapshotHandle = FACFMediaCatalog::GetSnapshot();
	const FACFMediaCatalogSnapshot& Snapshot = *SnapshotHandle;
	const bool bYarinConnected = FACFMediaGateway::IsPresent();
	bool bBalanceTaken = false;
	for (const TSharedRef<IACFGenerationSection>& Section : FACFGenerationSections::GetAll())
	{
		const FACFGenerationCardState State = ComputeGenerationCardState(Section->ReadEntitlement(Snapshot), Snapshot,
			bYarinConnected, Section->GetSetting(), Section->HasOwnKey());
		const bool bCarriesBalance = State.bShowBalance && !bBalanceTaken;
		bBalanceTaken |= State.bShowBalance;
		CardList->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
		[
			SNew(SACFGenerationCard).Section(Section).ShowBalance(bCarriesBalance)
		];
	}
	// No section is registered (a module still starting up): the widget stays empty until one registers.
}
