// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "GenerationSections.h"

#include "ThreeDGenerationSection.h"
#include "HAL/CriticalSection.h"
#include "UObject/Class.h"

namespace
{
	FCriticalSection GSectionsLock;
	TArray<TSharedRef<IACFGenerationSection>> GSections;
	FACFGenerationSectionsChanged GSectionsChanged;
	FACFGenerationRequestDelegate GOpenPlanPage;

	void BroadcastChanged()
	{
		GSectionsChanged.Broadcast();
	}
}

FACFGenerationCardState ComputeGenerationCardState(
	const FACFGenerationEntitlement& SectionEntitlement,
	const FACFMediaCatalogSnapshot& Snapshot,
	const bool bYarinConnected,
	const EACF3DKeySource Setting,
	const bool bHasOwnKey)
{
	FACFGenerationCardState State;

	// The two connection conditions of the Yarin option, per the owner's rules.
	const bool bEntitled = SectionEntitlement.bEntitled;
	State.bYarinEnabled = bYarinConnected && bEntitled;
	State.bYarinLocked = !State.bYarinEnabled;
	State.LockReason = bYarinConnected
		? (SectionEntitlement.RequiredPlan.IsEmpty()
			? TEXT("Your Yarin plan does not include this feature.")
			: FString::Printf(TEXT("Included from %s"), *SectionEntitlement.RequiredPlan))
		: TEXT("Sign in to Yarin");

	const bool bReady = Snapshot.State == EACFMediaCatalogState::Ready;
	State.bShowPrices = bYarinConnected && bEntitled && bReady && SectionEntitlement.bHasPrice;
	State.bShowBalance = bYarinConnected && bEntitled && bReady && Snapshot.bHasRemaining;
	State.bShowRetryLink = bYarinConnected && Snapshot.State == EACFMediaCatalogState::Failed;
	State.bShowLoadingNote = bYarinConnected && bEntitled && Snapshot.State == EACFMediaCatalogState::Loading;

	// The RECOMMENDED badge marks the Yarin option as the effective default right now.
	State.bYarinRecommended = Setting == EACF3DKeySource::Auto && !bHasOwnKey && State.bYarinEnabled;

	// Rule (d): Auto is never a choice of its own.
	switch (Setting)
	{
	case EACF3DKeySource::OwnKey:
		State.EffectiveSource = EACFGenerationSource::OwnKey;
		break;
	case EACF3DKeySource::Yarin:
		State.EffectiveSource = EACFGenerationSource::Yarin;
		break;
	case EACF3DKeySource::Auto:
	default:
		if (bHasOwnKey)
		{
			State.EffectiveSource = EACFGenerationSource::OwnKey;
		}
		else if (bYarinConnected && bEntitled)
		{
			State.EffectiveSource = EACFGenerationSource::Yarin;
		}
		else
		{
			State.EffectiveSource = EACFGenerationSource::OwnKey;
		}
		break;
	}
	const bool bYarinChosen = State.EffectiveSource == EACFGenerationSource::Yarin;

	// Rule (e): never a silent fallback. When the chosen Yarin source cannot serve, say why and offer both ways out.
	const bool bHasRemaining = bYarinConnected && bEntitled && bReady && Snapshot.bHasRemaining;
	const bool bOutOfCredit = bHasRemaining && SectionEntitlement.bHasPrice && Snapshot.RemainingCredits < SectionEntitlement.CheapestCredits;
	const bool bYarinCannotServe = !bYarinConnected || !bEntitled || Snapshot.bBlocked || bOutOfCredit;
	State.bShowFallbackMessage = bYarinChosen && bYarinCannotServe;
	if (State.bShowFallbackMessage)
	{
		if (Snapshot.bBlocked)
		{
			State.FallbackReason = TEXT("Yarin cannot bill this account right now. Use your own key or add credit.");
		}
		else if (bOutOfCredit)
		{
			State.FallbackReason = FString::Printf(TEXT("Only %s. The cheapest option costs %s, so switch to your own key or add credit."),
				*FACFMediaCatalog::FormatCreditsLeftThisMonth(Snapshot.RemainingCredits),
				*FACFMediaCatalog::FormatAboutCredits(SectionEntitlement.CheapestCredits));
		}
		else if (!bYarinConnected)
		{
			State.FallbackReason = TEXT("Sign in to Yarin to bill this generation to your credit, or use your own key.");
		}
		else
		{
			State.FallbackReason = FString::Printf(TEXT("Your Yarin plan does not include this feature. It is included from %s. Use your own key or upgrade your plan."),
				*SectionEntitlement.RequiredPlan);
		}
	}

	// Low but affordable: the balance still covers the cheapest option, once at most.
	const bool bLowButAffordable = bHasRemaining && SectionEntitlement.bHasPrice
		&& Snapshot.RemainingCredits >= SectionEntitlement.CheapestCredits
		&& Snapshot.RemainingCredits < 2 * SectionEntitlement.CheapestCredits;
	State.bShowLowCreditWarning = bLowButAffordable;

	return State;
}

namespace
{
	/** "Meshy" as the enum names the provider. Built once: the note is re-evaluated on every panel tick. */
	FString ProviderDisplayNameOf(const EACF3DProvider Provider)
	{
		static const TArray<FString> Names = []
		{
			TArray<FString> Built;
			if (const UEnum* ProviderEnum = StaticEnum<EACF3DProvider>())
			{
				for (int32 Index = 0; Index < ProviderEnum->NumEnums() - 1; ++Index)
				{
					Built.Add(ProviderEnum->GetDisplayNameTextByIndex(Index).ToString());
				}
			}
			return Built;
		}();
		const int32 Index = FMath::Clamp(static_cast<int32>(Provider), 0, FMath::Max(Names.Num() - 1, 0));
		return Names.IsValidIndex(Index) ? Names[Index] : FThreeDGenerationSection::CatalogIdOf(Provider);
	}

	/**
	 * The price the catalog bills for one mesh call, margin inclusive: false when no line names it. The catalog line
	 * is already the whole job: Meshy's textured text to 3D line includes its preview and its texture pass, so the
	 * line is read as is and never added to another.
	 */
	bool CallPriceCredits(const EACF3DProvider Provider, const bool bImageBased, const bool bTexturesOn,
		const FACFMediaCatalogSnapshot& Snapshot, int64& OutCredits)
	{
		const FString LineId = FString::Printf(TEXT("%s-%s"), bImageBased ? TEXT("image") : TEXT("text"),
			bTexturesOn ? TEXT("textured") : TEXT("mesh"));
		const FACFCatalogLine* Line = FACFMediaCatalog::FindLine(Snapshot, FThreeDGenerationSection::CatalogIdOf(Provider), LineId);
		if (!Line)
		{
			return false;
		}
		OutCredits = Line->Credits;
		return true;
	}
}

FString ComputeMeshApprovalNote(
	const EACF3DProvider Provider,
	const bool bImageBased,
	const bool bTexturesOn,
	const FACFMediaCatalogSnapshot& Snapshot,
	const bool bYarinConnected,
	const EACF3DKeySource Setting,
	const bool bHasOwnKey,
	const FACFGenerationEntitlement& SectionEntitlement)
{
	// The source decision is the card's own resolver; this note never decides it a second time.
	const FACFGenerationCardState State = ComputeGenerationCardState(SectionEntitlement, Snapshot,
		bYarinConnected, Setting, bHasOwnKey);
	const FString ProviderName = ProviderDisplayNameOf(Provider);
	if (State.EffectiveSource == EACFGenerationSource::OwnKey)
	{
		// Auto may resolve here with no key saved at all: name nothing that is not true, so no note appears.
		return bHasOwnKey ? FString::Printf(TEXT("Uses your own %s key."), *ProviderName) : FString();
	}
	if (State.bShowFallbackMessage)
	{
		// The chosen Yarin source cannot serve: the card's plain refusal is the note.
		return State.FallbackReason;
	}
	if (!State.bShowPrices)
	{
		// Not signed in, not entitled, a Loading or Failed catalog or no line at all: no amount is invented.
		return FString();
	}
	int64 Credits = 0;
	if (!CallPriceCredits(Provider, bImageBased, bTexturesOn, Snapshot, Credits))
	{
		return FString();
	}
	return FString::Printf(TEXT("%s, %s, %s from your Yarin plan."),
		*ProviderName, bTexturesOn ? TEXT("textured") : TEXT("untextured"), *FACFMediaCatalog::FormatAboutCredits(Credits));
}

void FACFGenerationSections::Register(const TSharedRef<IACFGenerationSection> Section)
{
	bool bChanged = false;
	{
		FScopeLock Lock(&GSectionsLock);
		for (TSharedRef<IACFGenerationSection>& Existing : GSections)
		{
			if (Existing->GetId() == Section->GetId())
			{
				// A re-register replaces the section in place, keeping the original order.
				Existing = Section;
				bChanged = true;
				break;
			}
		}
		if (!bChanged)
		{
			GSections.Add(Section);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		BroadcastChanged();
	}
}

void FACFGenerationSections::Unregister(const FName& Id)
{
	bool bRemoved = false;
	{
		FScopeLock Lock(&GSectionsLock);
		const int32 Index = GSections.IndexOfByPredicate([&Id](const TSharedRef<IACFGenerationSection>& Section)
		{
			return Section->GetId() == Id;
		});
		if (Index != INDEX_NONE)
		{
			GSections.RemoveAt(Index);
			bRemoved = true;
		}
	}
	if (bRemoved)
	{
		BroadcastChanged();
	}
}

TArray<TSharedRef<IACFGenerationSection>> FACFGenerationSections::GetAll()
{
	FScopeLock Lock(&GSectionsLock);
	return GSections;
}

FACFGenerationSectionsChanged& FACFGenerationSections::OnChanged()
{
	return GSectionsChanged;
}

FACFGenerationRequestDelegate& FACFGenerationSections::OnRequestOpenPlanPage()
{
	return GOpenPlanPage;
}
