// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "ADSVoiceGenerationSection.h"
#include "ADSVoicePayer.h"
#include "ADSYarinVoicePayer.h"

/**
 * Plugs the Yarin credit into the dialogue editor: the payer that generates voice from it and the voice card of the
 * shared Generation settings. Both registries are static and live in modules this one depends on, so the order in
 * which modules start does not matter. A project without this module has the own key path alone.
 */
class FAscentDialogueYarinModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Payer = MakeShared<FADSYarinVoicePayer>();
		ADSVoicePayers::Register(Payer.ToSharedRef());
		ADSVoiceSections::RegisterVoiceSection();
	}

	virtual void ShutdownModule() override
	{
		ADSVoiceSections::UnregisterVoiceSection();
		if (Payer.IsValid())
		{
			ADSVoicePayers::Unregister(Payer.ToSharedRef());
			Payer.Reset();
		}
	}

private:
	TSharedPtr<FADSYarinVoicePayer> Payer;
};

IMPLEMENT_MODULE(FAscentDialogueYarinModule, AscentDialogueYarin)
