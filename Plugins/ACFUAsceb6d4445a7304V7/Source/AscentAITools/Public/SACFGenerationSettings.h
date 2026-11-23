// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class IACFGenerationSection;

/**
 * One generation card per registered section, stacked in registration order. Both the AI Tools window and the
 * assistant panel drop this widget into their layout instead of hand building provider, key source and key
 * rows. Opening it also refreshes a stale catalog snapshot, so prices are never shown stale.
 */
class ASCENTAITOOLS_API SACFGenerationSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFGenerationSettings) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SACFGenerationSettings() override;

private:
	/** Rebuilds one card per registered section. */
	void RebuildContent();

	TSharedPtr<SVerticalBox> CardList;
	FDelegateHandle SectionsChangedHandle;
	FDelegateHandle CatalogChangedHandle;
};
