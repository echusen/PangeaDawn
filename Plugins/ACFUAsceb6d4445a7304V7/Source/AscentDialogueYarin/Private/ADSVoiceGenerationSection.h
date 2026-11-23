// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenerationSections.h"

/**
 * The voice generation section, registered for the dialogue editor by the AscentDialogueYarin module
 * startup. The Yarin body offers the ElevenLabs models of the catalog at their price and no voice choice, because
 * each line generates with the voice its own field provides; the own key body holds the ElevenLabs key field and
 * the model choice, the same settings the Yarin payer's billing rule reads.
 */
class FADSVoiceGenerationSection : public IACFGenerationSection
{
public:
	static const FName SectionId;

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

namespace ADSVoiceSections
{
	/** Registers the voice section with FACFGenerationSections (AscentDialogueYarin module startup). */
	void RegisterVoiceSection();

	/** Removes it again (module shutdown). */
	void UnregisterVoiceSection();
}
