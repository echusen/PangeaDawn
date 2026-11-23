// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenerationSections.h"

/**
 * The built-in 3D mesh generation section. The Yarin body offers the providers the catalog marks available
 * plus the price of the current texture setting; the own key body offers the provider dropdown and the API key
 * field, exactly the settings the 3D resolver reads.
 */
class FThreeDGenerationSection : public IACFGenerationSection
{
public:
	static const FName SectionId;

	/** The provider slug the media3d catalog uses for a 3D provider ("meshy", "tripo", "wavespeed"). */
	static FString CatalogIdOf(EACF3DProvider Provider);

	// IACFGenerationSection
	virtual FName GetId() const override;
	virtual FText GetTitle() const override;
	virtual bool HasYarinOption() const override;
	virtual EACF3DKeySource GetSetting() const override;
	virtual void SetSetting(EACF3DKeySource Source) override;
	virtual bool HasOwnKey() const override;
	virtual FACFGenerationEntitlement ReadEntitlement(const FACFMediaCatalogSnapshot& Snapshot) const override;
	virtual TSharedRef<SWidget> BuildYarinBody(const FACFMediaCatalogSnapshot& Snapshot) override;
	virtual TSharedRef<SWidget> BuildOwnKeyBody() override;
};

namespace ACF3D
{
	/** Registers the built-in 3D section with FACFGenerationSections (AscentAITools module startup). */
	ASCENTAITOOLS_API void RegisterGenerationSection();

	/** Removes it again (module shutdown). */
	ASCENTAITOOLS_API void UnregisterGenerationSection();
}
