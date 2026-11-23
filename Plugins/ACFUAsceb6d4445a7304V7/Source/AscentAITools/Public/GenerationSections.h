// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AscentAIToolsSettings.h"
#include "MediaCatalog.h"

class SWidget;

/** What one section reads out of a catalog snapshot for the card's shared state logic. */
struct FACFGenerationEntitlement
{
	bool bEntitled = false;
	/** The plan that includes the feature, shown when the Yarin option is locked ("Included from Hobbyist"). */
	FString RequiredPlan;
	/** The section's cheapest billable amount, margin inclusive. Valid only when bHasPrice. */
	int64 CheapestCredits = 0;
	bool bHasPrice = false;
};

/** One side of a card's two way choice. */
enum class EACFGenerationSource : uint8
{
	Yarin,
	OwnKey
};

/** The result of the card's rules, computed by the one pure function below. */
struct FACFGenerationCardState
{
	/** The source the card shows the body of: the effective default when Auto, otherwise the explicit choice. */
	EACFGenerationSource EffectiveSource = EACFGenerationSource::OwnKey;

	/** The Yarin option is selectable (connected and entitled). */
	bool bYarinEnabled = false;
	/** The Yarin option is dimmed with a padlock and disabled. */
	bool bYarinLocked = false;
	/** The Yarin option carries the RECOMMENDED badge: it is the effective default right now. */
	bool bYarinRecommended = false;
	/** Why the Yarin option is locked, shown under it ("Included from Hobbyist", "Sign in to Yarin"). */
	FString LockReason;

	/** Price lines come from the catalog (connected, entitled, a Ready catalog, and prices exist). */
	bool bShowPrices = false;
	/** The balance line shows ("7,400 credits left this month"). */
	bool bShowBalance = false;
	/** The balance is low but still covers the cheapest option: one more generation at most. */
	bool bShowLowCreditWarning = false;
	/** The chosen Yarin source cannot serve: an inline message with "Add credit" and "Use my key" shows. */
	bool bShowFallbackMessage = false;
	/** Plain reason the fallback message shows. */
	FString FallbackReason;
	/** The catalog fetch failed: a quiet retry link shows and no prices appear. */
	bool bShowRetryLink = false;
	/** A first catalog fetch is running: a quiet "Fetching prices..." note shows. */
	bool bShowLoadingNote = false;
};

/**
 * One billable generation feature shown as a card: 3D mesh generation today, voice later. A section owns
 * nothing about the other sections and nothing about Yarin sign in; it reads Project Settings, builds its two
 * bodies and reads what a catalog snapshot entitles it to. The card widget drives the shared choice logic.
 */
class ASCENTAITOOLS_API IACFGenerationSection
{
public:
	virtual ~IACFGenerationSection() = default;

	/** Stable registry id, for example "3d". */
	virtual FName GetId() const = 0;

	/** Card title, for example "3D Mesh Generation". */
	virtual FText GetTitle() const = 0;

	/** True when the section offers the Yarin credit option at all. */
	virtual bool HasYarinOption() const = 0;

	/** The saved billed-to choice. */
	virtual EACF3DKeySource GetSetting() const = 0;

	/** Stores the billed-to choice in Project Settings and persists it. */
	virtual void SetSetting(EACF3DKeySource Source) = 0;

	/** True when an own key for this section is saved, which is what Auto resolves to first. */
	virtual bool HasOwnKey() const = 0;

	/**
	 * What this section reads out of a catalog snapshot: whether it is entitled, the plan that includes it
	 * when not, and its cheapest billable amount (margin inclusive) so the card can warn before it is spent.
	 */
	virtual FACFGenerationEntitlement ReadEntitlement(const FACFMediaCatalogSnapshot& Snapshot) const = 0;

	/** The body shown while the Yarin credit side is chosen. */
	virtual TSharedRef<SWidget> BuildYarinBody(const FACFMediaCatalogSnapshot& Snapshot) = 0;

	/** The body shown while the own key side is chosen. */
	virtual TSharedRef<SWidget> BuildOwnKeyBody() = 0;
};

/**
 * Pure: the whole state of one generation card from its inputs, implementing the shared rules exactly:
 * connected and entitled enables the Yarin option with balance and prices; connected without entitlement
 * locks it with "Included from <plan>"; not connected dims it with a padlock and "Sign in to Yarin"; Auto is
 * never a choice (own key when saved, else Yarin when connected and entitled, else own key); a chosen Yarin
 * source that cannot serve never falls back silently, it shows the fallback message; a Failed catalog shows
 * a retry link. Balance and prices are false whenever not connected or not entitled.
 */
ASCENTAITOOLS_API FACFGenerationCardState ComputeGenerationCardState(
	const FACFGenerationEntitlement& SectionEntitlement,
	const FACFMediaCatalogSnapshot& Snapshot,
	bool bYarinConnected,
	EACF3DKeySource Setting,
	bool bHasOwnKey);

/** Registry id of the built-in 3D mesh generation section ("3d"), shared by callers that find it through the registry. */
inline const FName& ThreeDMeshSectionId()
{
	static const FName Id(TEXT("3d"));
	return Id;
}

/**
 * Pure: the note under a mesh tool call on a change approval card, reusing ComputeGenerationCardState for the
 * source decision. From your Yarin credit with a Ready and entitled catalog it names the provider, the texture
 * setting and the catalog line of this call, margin inclusive; the catalog line is the whole job (Meshy's textured
 * text to 3D line already includes its preview and texture tasks), so it is read as is. From your own key it names
 * the key and carries no price. A chosen Yarin source
 * that cannot serve carries the same plain refusal the generation card shows, and no amount appears in any
 * other case: the catalog and an entitled account are the only price sources.
 */
ASCENTAITOOLS_API FString ComputeMeshApprovalNote(
	const EACF3DProvider Provider,
	const bool bImageBased,
	const bool bTexturesOn,
	const FACFMediaCatalogSnapshot& Snapshot,
	const bool bYarinConnected,
	const EACF3DKeySource Setting,
	const bool bHasOwnKey,
	const FACFGenerationEntitlement& SectionEntitlement);

/** Fired on the game thread when the set of registered sections changes. */
DECLARE_MULTICAST_DELEGATE(FACFGenerationSectionsChanged);

/** Fired when a user action asks for the Yarin plan page. AscentACFAssistant binds it. */
DECLARE_MULTICAST_DELEGATE(FACFGenerationRequestDelegate);

/**
 * Registry of the generation sections. AscentAITools registers the built in 3D section at module startup;
 * other modules register theirs the same way. Nothing here knows what a section is about.
 */
class ASCENTAITOOLS_API FACFGenerationSections
{
public:
	static void Register(TSharedRef<IACFGenerationSection> Section);

	/** Removes the section with the id. Unknown ids stay silent. */
	static void Unregister(const FName& Id);

	/** The registered sections in registration order. */
	static TArray<TSharedRef<IACFGenerationSection>> GetAll();

	/** Broadcast on the game thread after Register or Unregister. */
	static FACFGenerationSectionsChanged& OnChanged();

	/** Fired when a card's "Add credit" button is pressed; the assistant binds the plan page url. */
	static FACFGenerationRequestDelegate& OnRequestOpenPlanPage();
};
