// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ThreeDGenerationSection.h"

#include "AscentAIToolsSettings.h"
#include "MediaCatalog.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "AscentAITools"

namespace
{
	/** The option names of the provider dropdown, in enum order. */
	TArray<TSharedPtr<FString>>& ProviderOptions()
	{
		static TArray<TSharedPtr<FString>> Options = []
		{
			TArray<TSharedPtr<FString>> Built;
			if (const UEnum* ProviderEnum = StaticEnum<EACF3DProvider>())
			{
				for (int32 Index = 0; Index < ProviderEnum->NumEnums() - 1; ++Index)
				{
					Built.Add(MakeShared<FString>(ProviderEnum->GetDisplayNameTextByIndex(Index).ToString()));
				}
			}
			if (Built.IsEmpty())
			{
				Built.Add(MakeShared<FString>(TEXT("Meshy")));
			}
			return Built;
		}();
		return Options;
	}

	void StoreProvider(const int32 OptionIndex)
	{
		UACFAIToolsSettings* Settings = GetMutableDefault<UACFAIToolsSettings>();
		Settings->ThreeDProvider = static_cast<EACF3DProvider>(FMath::Clamp(OptionIndex,
			0, static_cast<int32>(EACF3DProvider::WaveSpeed)));
		Settings->TryUpdateDefaultConfigFile();
	}

	/** True when the catalog marks the provider available, false when it says nothing or marks it unavailable. */
	bool IsProviderAvailable(const EACF3DProvider Provider)
	{
		const TSharedRef<const FACFMediaCatalogSnapshot> Snapshot = FACFMediaCatalog::GetSnapshot();
		for (const FACFCatalogProvider& Entry : Snapshot->ThreeDProviders)
		{
			if (Entry.Id == FThreeDGenerationSection::CatalogIdOf(Provider))
			{
				return Entry.bAvailable;
			}
		}
		return false;
	}

	FText ProviderDisplayName()
	{
		if (const UEnum* ProviderEnum = StaticEnum<EACF3DProvider>())
		{
			return ProviderEnum->GetDisplayNameTextByValue(static_cast<int64>(UACFAIToolsSettings::Get()->ThreeDProvider));
		}
		return FText::GetEmpty();
	}

	/** The provider row shared by both bodies; the row note marks what the catalog says about availability. */
	TSharedRef<SWidget> MakeProviderRow(const bool bForYarin)
	{
		TArray<TSharedPtr<FString>>& Options = ProviderOptions();
		const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
		const int32 Selected = FMath::Clamp(static_cast<int32>(Settings->ThreeDProvider), 0, Options.Num() - 1);

		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
		[
			SNew(STextBlock).Text(LOCTEXT("ProviderLabel", "Provider"))
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox).MinDesiredWidth(160.0f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&Options)
				.InitiallySelectedItem(Options[Selected])
				.ToolTipText(bForYarin
					? LOCTEXT("YarinProviderTip", "Providers billed to your Yarin credit. One marked unavailable has no key on the Yarin service yet.")
					: LOCTEXT("OwnKeyProviderTip", "The provider your own key bills. Each provider keeps its own key and base URL under Advanced in Project Settings."))
				.OnGenerateWidget_Lambda([bForYarin](TSharedPtr<FString> Option)
				{
					const int32 Index = ProviderOptions().IndexOfByKey(Option);
					const bool bAvailable = !bForYarin || Index == INDEX_NONE
						|| IsProviderAvailable(static_cast<EACF3DProvider>(Index));
					return SNew(STextBlock).Text(Option.IsValid()
						? FText::FromString(bAvailable ? *Option : *Option + TEXT(" (unavailable right now)"))
						: FText::GetEmpty());
				})
				.OnSelectionChanged_Lambda([](TSharedPtr<FString> Option, ESelectInfo::Type)
				{
					const int32 Index = ProviderOptions().IndexOfByKey(Option);
					if (Index != INDEX_NONE)
					{
						StoreProvider(Index);
					}
				})
				[
					SNew(STextBlock).Text_Lambda([] { return ProviderDisplayName(); })
				]
			]
		];
	}
}

const FName FThreeDGenerationSection::SectionId = ThreeDMeshSectionId();

FString FThreeDGenerationSection::CatalogIdOf(const EACF3DProvider Provider)
{
	switch (Provider)
	{
	case EACF3DProvider::Tripo: return TEXT("tripo");
	case EACF3DProvider::WaveSpeed: return TEXT("wavespeed");
	case EACF3DProvider::Meshy:
	default: return TEXT("meshy");
	}
}

FName FThreeDGenerationSection::GetId() const
{
	return SectionId;
}

FText FThreeDGenerationSection::GetTitle() const
{
	return LOCTEXT("ThreeDSectionTitle", "3D Mesh Generation");
}

bool FThreeDGenerationSection::HasYarinOption() const
{
	return true;
}

EACF3DKeySource FThreeDGenerationSection::GetSetting() const
{
	return UACFAIToolsSettings::Get()->ThreeDKeySource;
}

void FThreeDGenerationSection::SetSetting(const EACF3DKeySource Source)
{
	UACFAIToolsSettings* Settings = GetMutableDefault<UACFAIToolsSettings>();
	Settings->ThreeDKeySource = Source;
	Settings->TryUpdateDefaultConfigFile();
}

bool FThreeDGenerationSection::HasOwnKey() const
{
	return !UACFAIToolsSettings::Get()->GetActiveThreeDApiKey().TrimStartAndEnd().IsEmpty();
}

FACFGenerationEntitlement FThreeDGenerationSection::ReadEntitlement(const FACFMediaCatalogSnapshot& Snapshot) const
{
	FACFGenerationEntitlement Out;
	Out.bEntitled = Snapshot.bThreeDEntitled;
	Out.RequiredPlan = Snapshot.RequiredPlan;

	// The cheapest line the catalog bills this account: the floor a remaining balance has to clear.
	int64 Cheapest = TNumericLimits<int64>::Max();
	for (const FACFCatalogProvider& Provider : Snapshot.ThreeDProviders)
	{
		if (!Provider.bAvailable)
		{
			continue;
		}
		for (const FACFCatalogLine& Line : Provider.Lines)
		{
			Cheapest = FMath::Min(Cheapest, Line.Credits);
		}
	}
	Out.bHasPrice = Cheapest != TNumericLimits<int64>::Max();
	Out.CheapestCredits = Out.bHasPrice ? Cheapest : 0;
	return Out;
}

TSharedRef<SWidget> FThreeDGenerationSection::BuildYarinBody(const FACFMediaCatalogSnapshot& /*Snapshot*/)
{
	return SNew(SVerticalBox)

		// The provider dropdown, marked against the catalog.
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			MakeProviderRow(/*bForYarin*/ true)
		]

		// The selected provider is not available on the Yarin service right now.
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			SNew(STextBlock)
			.Visibility_Lambda([]
			{
				return IsProviderAvailable(UACFAIToolsSettings::Get()->ThreeDProvider)
					? EVisibility::Collapsed : EVisibility::Visible;
			})
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 8))
			.Text(LOCTEXT("ProviderUnavailable", "This provider is not available on the Yarin service right now. Pick another one."))
		]

		// The price of the line matching the current texture setting, margin inclusive.
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.ToolTipText(LOCTEXT("QualityLineTip", "What your Yarin credit pays for one mesh at the current texture setting. The margin is included."))
			.Text_Lambda([]
			{
				const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
				const FString LineId = Settings->bMeshyGenerateTextures ? TEXT("text-textured") : TEXT("text-mesh");
				// The handle outlives Line: the pointer points into the snapshot the handle keeps alive.
				const TSharedRef<const FACFMediaCatalogSnapshot> Snapshot = FACFMediaCatalog::GetSnapshot();
				const FACFCatalogLine* Line = FACFMediaCatalog::FindLine(*Snapshot,
					FThreeDGenerationSection::CatalogIdOf(Settings->ThreeDProvider), LineId);
				if (!Line)
				{
					return FText::GetEmpty();
				}
				return FText::FromString(FString::Printf(TEXT("%s per mesh"), *FACFMediaCatalog::FormatAboutCredits(Line->Credits)));
			})
			.Visibility_Lambda([]
			{
				const UACFAIToolsSettings* Settings = UACFAIToolsSettings::Get();
				const FString LineId = Settings->bMeshyGenerateTextures ? TEXT("text-textured") : TEXT("text-mesh");
				const TSharedRef<const FACFMediaCatalogSnapshot> Snapshot = FACFMediaCatalog::GetSnapshot();
				return FACFMediaCatalog::FindLine(*Snapshot, FThreeDGenerationSection::CatalogIdOf(Settings->ThreeDProvider), LineId)
					? EVisibility::Visible : EVisibility::Collapsed;
			})
		];
}

TSharedRef<SWidget> FThreeDGenerationSection::BuildOwnKeyBody()
{
	return SNew(SVerticalBox)

		// The provider dropdown, same setting the resolver reads for its own key path.
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
		[
			MakeProviderRow(/*bForYarin*/ false)
		]

		// The API key of the selected provider.
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SEditableTextBox)
			.IsPassword(true)
			.HintText(LOCTEXT("OwnKeyHint", "Key of the selected provider; each provider keeps its own key"))
			.ToolTipText(LOCTEXT("OwnKeyTip", "Paste the API key from the provider's account page. It is stored in Project Settings."))
			.Text_Lambda([]
			{
				return FText::FromString(UACFAIToolsSettings::Get()->GetActiveThreeDApiKey());
			})
			.OnTextCommitted_Lambda([](const FText& Text, ETextCommit::Type)
			{
				UACFAIToolsSettings* Settings = GetMutableDefault<UACFAIToolsSettings>();
				Settings->SetActiveThreeDApiKey(Text.ToString());
				Settings->TryUpdateDefaultConfigFile();
			})
		];
}

void ACF3D::RegisterGenerationSection()
{
	static const TSharedRef<FThreeDGenerationSection> Section = MakeShared<FThreeDGenerationSection>();
	FACFGenerationSections::Register(Section);
}

void ACF3D::UnregisterGenerationSection()
{
	FACFGenerationSections::Unregister(FThreeDGenerationSection::SectionId);
}

#undef LOCTEXT_NAMESPACE
