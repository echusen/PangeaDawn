// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSVoicePayer.h"
#include "Misc/AutomationTest.h"

namespace
{
	/** A payer whose decision the test controls. It never generates anything. */
	class FFakePayer : public IADSVoicePayer
	{
	public:
		bool bPaying = false;

		virtual bool IsPaying() const override { return bPaying; }
		virtual void RequestSpeech(const FString&, const FString&, bool,
			TFunction<void(bool, const TArray<uint8>&, const FString&)>) override {}
		virtual EADSBatchVerdict ReviewBatch(int32, int32, FString&) override { return EADSBatchVerdict::Proceed; }
		virtual void BatchFinished(int32) override {}
		virtual bool ListVoices(TArray<FADSPayerVoice>&, FString&, FString&) override { return false; }
	};

	/** Puts the payer that was registered before the test back, whatever the test did to the registry. */
	struct FRegistryGuard
	{
		TSharedPtr<IADSVoicePayer> Previous = ADSVoicePayers::GetRegistered();

		FRegistryGuard()
		{
			if (Previous.IsValid())
			{
				ADSVoicePayers::Unregister(Previous.ToSharedRef());
			}
		}

		~FRegistryGuard()
		{
			if (const TSharedPtr<IADSVoicePayer> Current = ADSVoicePayers::GetRegistered())
			{
				ADSVoicePayers::Unregister(Current.ToSharedRef());
			}
			if (Previous.IsValid())
			{
				ADSVoicePayers::Register(Previous.ToSharedRef());
			}
		}
	};
}

/**
 * The own key path must stay the default: with no payer, or with one that is not paying, generation is routed
 * to nobody else. This is what a project that has no Yarin at all relies on.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FADSVoicePayerRegistryTest, "Ascent.DialogueEditor.VoicePayer.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FADSVoicePayerRegistryTest::RunTest(const FString& Parameters)
{
	FRegistryGuard Guard;

	TestTrue(TEXT("With no payer registered nobody else pays"), !ADSVoicePayers::GetPaying().IsValid());
	TestTrue(TEXT("With no payer registered nothing is registered"), !ADSVoicePayers::GetRegistered().IsValid());

	const TSharedRef<FFakePayer> First = MakeShared<FFakePayer>();
	ADSVoicePayers::Register(First);
	TestTrue(TEXT("A registered payer that is not paying leaves the own key path"), !ADSVoicePayers::GetPaying().IsValid());
	TestTrue(TEXT("A registered payer that is not paying is still registered"), ADSVoicePayers::GetRegistered() == First);

	First->bPaying = true;
	TestTrue(TEXT("The payer is returned the moment it is paying"), ADSVoicePayers::GetPaying() == First);

	First->bPaying = false;
	TestTrue(TEXT("The payer stops being returned the moment it stops paying"), !ADSVoicePayers::GetPaying().IsValid());

	// A second registration replaces the first; the first can no longer be returned.
	const TSharedRef<FFakePayer> Second = MakeShared<FFakePayer>();
	First->bPaying = true;
	Second->bPaying = true;
	ADSVoicePayers::Register(Second);
	TestTrue(TEXT("A second registration replaces the first"), ADSVoicePayers::GetPaying() == Second);

	// Unregistering a payer that is not the registered one must not remove the registered one.
	ADSVoicePayers::Unregister(First);
	TestTrue(TEXT("Unregistering a different payer leaves the registered one"), ADSVoicePayers::GetPaying() == Second);

	ADSVoicePayers::Unregister(Second);
	TestTrue(TEXT("Unregistering the registered payer leaves nobody paying"), !ADSVoicePayers::GetPaying().IsValid());
	return true;
}
