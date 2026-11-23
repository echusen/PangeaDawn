// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSVoiceGenerationSection.h"

#include "ADSYarinVoice.h"
#include "ADSDialogueDeveloperSettings.h"
#include "ADSYarinVoiceSettings.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "AscentDialogueVoiceSection"

namespace
{
	EACF3DKeySource KeySourceOf(const EADSVoiceBillingSource Billing)
	{
		switch (Billing)
		{
		case EADSVoiceBillingSource::OwnKey: return EACF3DKeySource::OwnKey;
		case EADSVoiceBillingSource::Yarin: return EACF3DKeySource::Yarin;
		case EADSVoiceBillingSource::Auto:
		default: return EACF3DKeySource::Auto;
		}
	}

	EADSVoiceBillingSource BillingSourceOf(const EACF3DKeySource Source)
	{
		switch (Source)
		{
		case EACF3DKeySource::OwnKey: return EADSVoiceBillingSource::OwnKey;
		case EACF3DKeySource::Yarin: return EADSVoiceBillingSource::Yarin;
		case EACF3DKeySource::Auto:
		default: return EADSVoiceBillingSource::Auto;
		}
	}

	/** The model ids of the own key side, the ones the settings recognize. */
	const TArray<FString>& OwnKeyModelIds()
	{
		static const TArray<FString> Models = { TEXT("eleven_v3"), TEXT("eleven_v4"), TEXT("eleven_v4_turbo") };
		return Models;
	}

	/** A display name for an own key model id ("eleven_v4_turbo" reads "ElevenLabs v4 Turbo"), the same names the Yarin side shows. */
	FString OwnKeyModelLabel(const FString& ModelId)
	{
		if (ModelId == TEXT("eleven_v3")) return TEXT("ElevenLabs v3");
		if (ModelId == TEXT("eleven_v4")) return TEXT("ElevenLabs v4");
		if (ModelId == TEXT("eleven_v4_turbo")) return TEXT("ElevenLabs v4 Turbo");
		return ModelId;
	}

	/** What the Yarin side calls a catalog model: the ElevenLabs name behind the id ("ElevenLabs v4"), the catalog's plain name when it gives none. */
	FString YarinModelLabel(const FACFCatalogVoiceModel& Model)
	{
		return Model.Detail.IsEmpty() ? Model.Name : Model.Detail;
	}

	void StoreOwnKeyModel(const FString& ModelId)
	{
		UADSDialogueDeveloperSettings* Settings = GetMutableDefault<UADSDialogueDeveloperSettings>();
		Settings->SetTTSVoiceModel(ModelId);
		Settings->TryUpdateDefaultConfigFile();
	}

	void StoreVoiceBillingSource(const EADSVoiceBillingSource Source)
	{
		UADSYarinVoiceSettings* Settings = GetMutableDefault<UADSYarinVoiceSettings>();
		Settings->SetVoiceBillingSource(Source);
		Settings->TryUpdateDefaultConfigFile();
	}

	void StoreOwnKey(const FString& Key)
	{
		UADSDialogueDeveloperSettings* Settings = GetMutableDefault<UADSDialogueDeveloperSettings>();
		Settings->SetTTSVoiceGenAPIKey(Key.TrimStartAndEnd());
		Settings->TryUpdateDefaultConfigFile();
	}

	void StoreYarinVoiceModel(const FString& ModelId)
	{
		UADSYarinVoiceSettings* Settings = GetMutableDefault<UADSYarinVoiceSettings>();
		Settings->SetYarinVoiceModelId(ModelId);
		Settings->TryUpdateDefaultConfigFile();
	}

	/** The model choices the Yarin card was last built with (the combo keeps a pointer in). */
	TArray<TSharedPtr<FACFCatalogVoiceModel>>& YarinModelOptions()
	{
		static TArray<TSharedPtr<FACFCatalogVoiceModel>> Options;
		return Options;
	}

	/** The own key model choices (same pointer lifetime as the card). */
	TArray<TSharedPtr<FString>>& OwnKeyModelOptions()
	{
		static TArray<TSharedPtr<FString>> Options;
		return Options;
	}
}

const FName FADSVoiceGenerationSection::SectionId(TEXT("voice"));

FName FADSVoiceGenerationSection::GetId() const
{
	return SectionId;
}

FText FADSVoiceGenerationSection::GetTitle() const
{
	return LOCTEXT("VoiceSectionTitle", "Voice Generation");
}

bool FADSVoiceGenerationSection::HasYarinOption() const
{
	return true;
}

EACF3DKeySource FADSVoiceGenerationSection::GetSetting() const
{
	return KeySourceOf(GetDefault<UADSYarinVoiceSettings>()->GetVoiceBillingSource());
}

void FADSVoiceGenerationSection::SetSetting(const EACF3DKeySource Source)
{
	StoreVoiceBillingSource(BillingSourceOf(Source));
}

bool FADSVoiceGenerationSection::HasOwnKey() const
{
	return !GetDefault<UADSDialogueDeveloperSettings>()->GetTTSVoiceGenAPIKey().TrimStartAndEnd().IsEmpty();
}

FACFGenerationEntitlement FADSVoiceGenerationSection::ReadEntitlement(const FACFMediaCatalogSnapshot& Snapshot) const
{
	FACFGenerationEntitlement Out;
	Out.bEntitled = Snapshot.bVoiceEntitled;
	Out.RequiredPlan = Snapshot.RequiredPlan;

	// The cheapest plausible amount at the shortest billable run: 100 characters at the cheapest model.
	int64 CheapestPer1k = TNumericLimits<int64>::Max();
	for (const FACFCatalogVoiceModel& Model : Snapshot.VoiceModels)
	{
		CheapestPer1k = FMath::Min(CheapestPer1k, Model.CreditsPer1kChars);
	}
	Out.bHasPrice = CheapestPer1k != TNumericLimits<int64>::Max();
	Out.CheapestCredits = Out.bHasPrice ? ADSYarinVoice::EstimateBatchCredits(100, CheapestPer1k) : 0;
	return Out;
}

TSharedRef<SWidget> FADSVoiceGenerationSection::BuildYarinBody(const FACFMediaCatalogSnapshot& Snapshot)
{
	// The models straight from the catalog, each named for the ElevenLabs version behind it and priced per
	// 1,000 characters. Held in a module static because the combo box keeps a pointer to the array for its lifetime.
	// There is no voice choice here: each dialogue line generates with the voice its own field provides.
	TArray<TSharedPtr<FACFCatalogVoiceModel>>& Models = YarinModelOptions();
	Models.Reset();
	for (const FACFCatalogVoiceModel& Model : Snapshot.VoiceModels)
	{
		Models.Add(MakeShared<FACFCatalogVoiceModel>(Model));
	}

	// The combo box shows the saved choice without a click. TSharedPtr equality is pointer equality,
	// so the shared item pointed at is the one the array above owns.
	const UADSYarinVoiceSettings* BuildSettings = GetDefault<UADSYarinVoiceSettings>();
	TSharedPtr<FACFCatalogVoiceModel> InitiallyChosenModel;
	for (TSharedPtr<FACFCatalogVoiceModel>& Model : Models)
	{
		if (Model->Id == BuildSettings->GetYarinVoiceModelId())
		{
			InitiallyChosenModel = Model;
			break;
		}
	}

	return SNew(SVerticalBox)

		// The model dropdown: the ElevenLabs versions of the catalog, each priced per 1,000 characters.
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock).Text(LOCTEXT("YarinModelLabel", "Model"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).MinDesiredWidth(160.0f)
				[
					SNew(SComboBox<TSharedPtr<FACFCatalogVoiceModel>>)
					.OptionsSource(&Models)
					.InitiallySelectedItem(InitiallyChosenModel.IsValid() ? InitiallyChosenModel : nullptr)
					.ToolTipText(LOCTEXT("YarinModelTip", "The ElevenLabs model your Yarin credit pays for, per 1,000 characters. The margin is included."))
					.OnGenerateWidget_Lambda([](TSharedPtr<FACFCatalogVoiceModel> Model)
					{
						if (!Model.IsValid())
						{
							return SNew(STextBlock).Text(FText::GetEmpty());
						}
						return SNew(STextBlock).Text(FText::FromString(FString::Printf(
							TEXT("%s (%s per 1,000 characters)"),
							*YarinModelLabel(*Model), *FACFMediaCatalog::FormatAboutCredits(Model->CreditsPer1kChars))));
					})
					.OnSelectionChanged_Lambda([](TSharedPtr<FACFCatalogVoiceModel> Model, ESelectInfo::Type)
					{
						if (Model.IsValid())
						{
							StoreYarinVoiceModel(Model->Id);
						}
					})
					[
						SNew(STextBlock).Text_Lambda([]
						{
							const UADSYarinVoiceSettings* Settings = GetDefault<UADSYarinVoiceSettings>();
							// The handle keeps the snapshot alive for as long as Model points into it.
							const TSharedRef<const FACFMediaCatalogSnapshot> Catalog = FACFMediaCatalog::GetSnapshot();
							const FACFCatalogVoiceModel* Model = ADSYarinVoice::FindVoiceModel(*Catalog, Settings->GetYarinVoiceModelId());
							return FText::FromString(Model ? YarinModelLabel(*Model) : Settings->GetYarinVoiceModelId());
						})
					]
				]
			]
		]

		// The price line of the chosen model.
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
			.Text_Lambda([]
			{
				const UADSYarinVoiceSettings* Settings = GetDefault<UADSYarinVoiceSettings>();
				const TSharedRef<const FACFMediaCatalogSnapshot> Catalog = FACFMediaCatalog::GetSnapshot();
				const FACFCatalogVoiceModel* Model = ADSYarinVoice::FindVoiceModel(*Catalog, Settings->GetYarinVoiceModelId());
				if (!Model)
				{
					return FText::GetEmpty();
				}
				return FText::FromString(FString::Printf(TEXT("%s per 1,000 characters"),
					*FACFMediaCatalog::FormatAboutCredits(Model->CreditsPer1kChars)));
			})
			.Visibility_Lambda([]
			{
				const UADSYarinVoiceSettings* Settings = GetDefault<UADSYarinVoiceSettings>();
				return ADSYarinVoice::FindVoiceModel(*FACFMediaCatalog::GetSnapshot(), Settings->GetYarinVoiceModelId())
					? EVisibility::Visible : EVisibility::Collapsed;
			})
		];
}

TSharedRef<SWidget> FADSVoiceGenerationSection::BuildOwnKeyBody()
{
	// Static so the combo box's pointer into the array survives past this call.
	TArray<TSharedPtr<FString>>& ModelOptions = OwnKeyModelOptions();
	ModelOptions.Reset();
	for (const FString& ModelId : OwnKeyModelIds())
	{
		ModelOptions.Add(MakeShared<FString>(ModelId));
	}

	TSharedPtr<FString> InitiallyChosenModel;
	for (TSharedPtr<FString>& Option : ModelOptions)
	{
		if (*Option == GetDefault<UADSDialogueDeveloperSettings>()->GetTTSVoiceModel())
		{
			InitiallyChosenModel = Option;
			break;
		}
	}

	return SNew(SVerticalBox)

		// The model choice of the own key side.
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock).Text(LOCTEXT("OwnModelLabel", "Model"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).MinDesiredWidth(160.0f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&ModelOptions)
					.InitiallySelectedItem(InitiallyChosenModel.IsValid() ? InitiallyChosenModel : nullptr)
					.ToolTipText(LOCTEXT("OwnModelTip", "The ElevenLabs model id sent as model_id for generations billed to your own key."))
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
					{
						return SNew(STextBlock).Text(Option.IsValid()
							? FText::FromString(OwnKeyModelLabel(*Option)) : FText::GetEmpty());
					})
					.OnSelectionChanged_Lambda([](TSharedPtr<FString> Option, ESelectInfo::Type)
					{
						if (Option.IsValid())
						{
							StoreOwnKeyModel(*Option);
						}
					})
					[
						SNew(STextBlock).Text_Lambda([]
						{
							return FText::FromString(OwnKeyModelLabel(GetDefault<UADSDialogueDeveloperSettings>()->GetTTSVoiceModel()));
						})
					]
				]
			]
		]

		// The existing ElevenLabs API key field.
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SEditableTextBox)
			.IsPassword(true)
			.HintText(LOCTEXT("OwnKeyHint", "Your ElevenLabs API key"))
			.ToolTipText(LOCTEXT("OwnKeyTip", "Paste the API key from your elevenlabs.io account page. It is stored in Project Settings."))
			.Text_Lambda([]
			{
				return FText::FromString(GetDefault<UADSDialogueDeveloperSettings>()->GetTTSVoiceGenAPIKey());
			})
			.OnTextCommitted_Lambda([](const FText& Text, ETextCommit::Type)
			{
				StoreOwnKey(Text.ToString());
			})
		];
}

void ADSVoiceSections::RegisterVoiceSection()
{
	static const TSharedRef<FADSVoiceGenerationSection> Section = MakeShared<FADSVoiceGenerationSection>();
	FACFGenerationSections::Register(Section);
}

void ADSVoiceSections::UnregisterVoiceSection()
{
	FACFGenerationSections::Unregister(FADSVoiceGenerationSection::SectionId);
}

#undef LOCTEXT_NAMESPACE
