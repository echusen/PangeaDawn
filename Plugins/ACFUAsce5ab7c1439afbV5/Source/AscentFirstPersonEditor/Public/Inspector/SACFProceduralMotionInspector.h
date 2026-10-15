// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/ACFProceduralMotionDebug.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class SWidget;
class UACFProceduralMotionComponent;

#if ACF_FP_DEBUG_CAPTURE

enum class EACFProceduralInspectorRowType : uint8
{
	StageHeader,
	Component,
	Op,
	/** A named value the component published through GetDebugState. */
	DebugValue,
	/** Stands in for a component that emitted nothing, so its expander arrow never comes and goes. */
	Placeholder
};

/**
 * One row in the stack tree.
 *
 * Rows are kept alive across refreshes and their payload is updated in place, so the tree never
 * regenerates its widgets while values change. Widgets bind to the row, not to a snapshot copy.
 */
struct FACFProceduralInspectorRow
{
	EACFProceduralInspectorRowType Type = EACFProceduralInspectorRowType::Op;

	int32 Stage = 0;

	TWeakObjectPtr<UACFProceduralMotionComponent> Component;

	FString ComponentName;

	/** Actor the component lives on. Only shown in the tooltip — several actors feed one stack. */
	FString OwnerName;

	/** Live op values. Meaningless on the other row types. */
	FACFProceduralMotionDebugOp Op;

	/** Ops this component emitted this frame. Component rows only. */
	int32 OpCount = 0;

	/** Live label and value. DebugValue rows only. */
	FACFProceduralMotionDebugValue Debug;

	/** Identity across refreshes. Rows with the same key are the same row. */
	FString Key;

	TArray<TSharedPtr<FACFProceduralInspectorRow>> Children;
};

/**
 * Live view of the motion stack: which component emits what, at which stage, with what weight.
 *
 * Reads FACFProceduralMotionDebugRegistry, so it needs no interface on the customer's classes and works on
 * whatever is currently evaluating. Turns ACF.FirstPerson.Debug.Capture on while it is open and off when it closes.
 */
class SACFProceduralMotionInspector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFProceduralMotionInspector) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual ~SACFProceduralMotionInspector() override;

	//~ Begin SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget

private:
	TSharedRef<SWidget> BuildHeader();

	TSharedRef<SWidget> BuildCurvePanel();

	TSharedRef<ITableRow> GenerateStackRow(TSharedPtr<FACFProceduralInspectorRow> Row, const TSharedRef<STableViewBase>& OwnerTable);

	void GetRowChildren(TSharedPtr<FACFProceduralInspectorRow> Row, TArray<TSharedPtr<FACFProceduralInspectorRow>>& OutChildren);

	void HandleExpansionChanged(TSharedPtr<FACFProceduralInspectorRow> Row, bool bExpanded);

	TSharedPtr<SWidget> BuildRowContextMenu();

	void SetAllExpansion(bool bExpand);

	/** One label plus three aligned number columns, so values line up in a narrow panel. */
	TSharedRef<SWidget> BuildChannelRow(const FText& Label, TSharedPtr<FACFProceduralInspectorRow> Row, bool bRotation) const;

	/** Pulls the newest snapshot and updates rows in place, rebuilding only when the row set changed. */
	void RefreshFromRegistry();

	void RebuildRows();

	/** Solo this component, or restore everything when it is already the only one enabled. */
	void ToggleSolo(UACFProceduralMotionComponent* Component);

	void SetAllEnabled(bool bEnabled);

	/** True when Component is the only enabled one on this snapshot. */
	bool IsSoloed(const UACFProceduralMotionComponent* Component) const;

	FText GetSourceText() const;

	TSharedRef<SWidget> BuildSourceMenu();

private:
	/** Mesh whose snapshot is being shown. Unset means "first available". */
	TWeakObjectPtr<const USkeletalMeshComponent> SelectedMesh;

	FACFProceduralMotionDebugSnapshot Snapshot;

	TArray<FACFProceduralMotionDebugSnapshot> AvailableSnapshots;

	/** Stage rows. Components hang off these, ops off those. */
	TArray<TSharedPtr<FACFProceduralInspectorRow>> RootRows;

	/** Expansion remembered by key, so a row recreated after a swap comes back as the user left it. */
	TMap<FString, bool> ExpansionByKey;

	/** Ordered key signature of the last build, to detect a real structural change. */
	FString RowSignature;

	TSharedPtr<STreeView<TSharedPtr<FACFProceduralInspectorRow>>> StackTree;

	/** Curve names the presets reference, so the readout is not every curve on the rig. */
	TSet<FName> InterestingCurves;

	double LastRefreshTime = 0.0;
};

#endif
