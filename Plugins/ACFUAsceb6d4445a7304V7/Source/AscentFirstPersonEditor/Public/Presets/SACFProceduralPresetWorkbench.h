// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SetupReport/ACFProceduralSetupTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class IDetailsView;
class SWidget;
class UACFProceduralPreset;
class UACFProceduralPresetData;
struct FAssetData;

enum class EACFProceduralPresetRowType : uint8
{
	/** "Bindings" and "Presets" roots. */
	Section,
	/** An actor grouping its bindings. */
	Actor,
	/** One preset data property on an actor or component. */
	Binding,
	/** A preset data asset grouping its presets. */
	PresetData,
	/** One preset inside a preset data asset. */
	Preset,
	/** Stands in for an empty group, so its expander arrow never comes and goes. */
	Placeholder
};

struct FACFProceduralPresetRow
{
	EACFProceduralPresetRowType Type = EACFProceduralPresetRowType::Section;

	FText Label;

	/** Binding rows. */
	FACFProceduralPresetBinding Binding;

	/** PresetData rows, and the current value of a Binding row. */
	TWeakObjectPtr<UACFProceduralPresetData> PresetData;

	/** Preset rows. */
	TWeakObjectPtr<UACFProceduralPreset> Preset;

	/** Binding rows: points somewhere different from its siblings on the same actor. */
	bool bDiffers = false;

	/** Identity across refreshes, so the tree keeps its widgets and expansion state. */
	FString Key;

	TArray<TSharedPtr<FACFProceduralPresetRow>> Children;
};

/**
 * Every preset data property reachable from one actor, grouped by owner, plus every preset inside those
 * assets, grouped by asset and editable in place.
 *
 * Preset data is a shared asset, so edits made while play-in-editor is running survive stopping it — this
 * is a tuning surface, not just an inspector. See Docs/EDITOR-TOOLING.md § 5.4.
 */
class ASCENTFIRSTPERSONEDITOR_API SACFProceduralPresetWorkbench : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFProceduralPresetWorkbench) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//~ Begin SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget

	/** Sets the asset or actor the user explicitly picked. */
	void SetPickedTarget(UObject* Target);

private:
	TSharedRef<SWidget> BuildHeader();

	/** Re-slots the persistent sub-widgets. Splitter runs vertical when narrow, horizontal when wide. */
	void RebuildLayout();

	TSharedRef<ITableRow> GenerateRow(TSharedPtr<FACFProceduralPresetRow> Row, const TSharedRef<STableViewBase>& OwnerTable);

	void GetRowChildren(TSharedPtr<FACFProceduralPresetRow> Row, TArray<TSharedPtr<FACFProceduralPresetRow>>& OutChildren);

	void HandleSelectionChanged(TSharedPtr<FACFProceduralPresetRow> Row, ESelectInfo::Type SelectInfo);

	/** Points the details view at whatever this row represents. No-op when it is already showing it. */
	void ShowRowInDetails(const TSharedPtr<FACFProceduralPresetRow>& Row);

	/**
	 * Re-applies the selection after a rebuild: same row if it survived, else the same preset class on the
	 * new equipment, else nothing.
	 */
	void RestoreSelection();

	void HandleExpansionChanged(TSharedPtr<FACFProceduralPresetRow> Row, bool bExpanded);

	TSharedPtr<SWidget> BuildRowContextMenu();

	void SetAllExpansion(bool bExpand);

	/** Rebuilds from the effective target, touching the tree only when its shape changed. */
	void Refresh();

	/** Play-in-editor pawn while following, otherwise whatever was picked. */
	UObject* ResolveEffectiveTarget() const;

	void UseCurrentSelection();

	FString GetPickedTargetPath() const;

	void HandleTargetChanged(const FAssetData& AssetData);

	/** Only actor blueprints carry preset data properties. */
	bool ShouldFilterAsset(const FAssetData& AssetData) const;

	void ApplyBinding(TSharedPtr<FACFProceduralPresetRow> Row, UACFProceduralPresetData* NewValue);

	/** Preset classes the components bound to PresetData read, that the asset does not contain. */
	TArray<TSubclassOf<UACFProceduralPreset>> GetMissingPresetClasses(UACFProceduralPresetData* PresetData) const;

	void AddMissingPresets(UACFProceduralPresetData* PresetData);

	FText GetSummaryText() const;

private:
	/** What the user chose. Kept while following play-in-editor, so stopping restores it. */
	TWeakObjectPtr<UObject> PickedTarget;

	TWeakObjectPtr<UObject> EffectiveTarget;

	FACFProceduralSetupContext Context;

	TArray<TSharedPtr<FACFProceduralPresetRow>> RootRows;

	/** Expansion remembered by key, so a row recreated after a swap comes back as the user left it. */
	TMap<FString, bool> ExpansionByKey;

	FString RowSignature;

	TSharedPtr<STreeView<TSharedPtr<FACFProceduralPresetRow>>> Tree;

	TSharedPtr<IDetailsView> DetailsView;

	TSharedPtr<SWidget> HeaderWidget;

	TWeakObjectPtr<UObject> DetailsObject;

	/** Key of the row the user last picked, so a rebuild can put the selection back where it was. */
	FString SelectedRowKey;

	/**
	 * Preset class the user last picked, kept even while no row matches it.
	 *
	 * Swapping to a weapon without that preset clears the visible selection, but swapping back restores it
	 * — the intent survives equipment that cannot honour it.
	 */
	TWeakObjectPtr<UClass> SelectedPresetClass;

	/** What the tree is actually showing, after filtering. */
	int32 ShownBindingCount = 0;

	int32 ShownPresetDataCount = 0;

	int32 HiddenBindingCount = 0;

	bool bFollowPlayInEditor = true;

	/** While playing, hide bindings and presets belonging to hidden actors — i.e. stowed equipment. */
	bool bOnlyVisibleActors = true;

	bool bWideLayout = false;

	double LastRefreshTime = 0.0;
};
