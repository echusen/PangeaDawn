// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Inspector/SACFProceduralMotionInspector.h"

#if ACF_FP_DEBUG_CAPTURE

#include "Components/ACFProceduralMotionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "Presets/ACFProceduralPreset.h"
#include "SetupReport/ACFProceduralSetupTypes.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ACFProceduralTreeExpansion.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ACFProceduralMotionInspector"

namespace ACFProceduralMotionInspector
{
	/** Polling rate. The stack changes every frame; refreshing Slate that fast is pure waste. */
	static const double RefreshInterval = 0.1;

	/** Width of the "L" / "R" channel labels, so both rows line up. */
	static const float ChannelLabelWidth = 16.0f;

	static IConsoleVariable* GetCaptureCVar()
	{
		return IConsoleManager::Get().FindConsoleVariable(TEXT("ACF.FirstPerson.Debug.Capture"));
	}

	static const TCHAR* CompositionToString(EACFProceduralMotionComposition Composition)
	{
		return Composition == EACFProceduralMotionComposition::Additive ? TEXT("Additive") : TEXT("LerpToTarget");
	}

	static const TCHAR* SpaceToString(EACFProceduralMotionOutputSpace Space)
	{
		switch (Space)
		{
		case EACFProceduralMotionOutputSpace::Bone:		return TEXT("Bone");
		case EACFProceduralMotionOutputSpace::ParentBone:	return TEXT("ParentBone");
		default:										return TEXT("Component");
		}
	}

	static FText FormatChannel(double Value)
	{
		return FText::FromString(FString::Printf(TEXT("%.1f"), Value));
	}

	/** Subdued unless the value actually does something, so a busy stack reads at a glance. */
	static FSlateColor GetChannelColor(double Value)
	{
		return FMath::IsNearlyZero(Value, 0.05)
			? FSlateColor::UseSubduedForeground()
			: FSlateColor::UseForeground();
	}
}

void SACFProceduralMotionInspector::Construct(const FArguments& InArgs)
{
	if (IConsoleVariable* CaptureCVar = ACFProceduralMotionInspector::GetCaptureCVar())
	{
		CaptureCVar->Set(1, ECVF_SetByConsole);
	}

	SAssignNew(StackTree, STreeView<TSharedPtr<FACFProceduralInspectorRow>>)
		.TreeItemsSource(&RootRows)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SACFProceduralMotionInspector::GenerateStackRow)
		.OnGetChildren(this, &SACFProceduralMotionInspector::GetRowChildren)
		.OnExpansionChanged(this, &SACFProceduralMotionInspector::HandleExpansionChanged)
		.OnContextMenuOpening(this, &SACFProceduralMotionInspector::BuildRowContextMenu);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 4.0f)
		[
			BuildHeader()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			StackTree.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildCurvePanel()
		]
	];
}

SACFProceduralMotionInspector::~SACFProceduralMotionInspector()
{
	if (IConsoleVariable* CaptureCVar = ACFProceduralMotionInspector::GetCaptureCVar())
	{
		CaptureCVar->Set(0, ECVF_SetByConsole);
	}
}

TSharedRef<SWidget> SACFProceduralMotionInspector::BuildHeader()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SACFProceduralMotionInspector::BuildSourceMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &SACFProceduralMotionInspector::GetSourceText)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ExpandAll", "Expand"))
				.ToolTipText(LOCTEXT("ExpandAllTooltip", "Expand every row. Right-click a row to do the same for just its subtree."))
				.OnClicked_Lambda([this]()
				{
					SetAllExpansion(true);
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CollapseAll", "Collapse"))
				.ToolTipText(LOCTEXT("CollapseAllTooltip", "Collapse every row."))
				.OnClicked_Lambda([this]()
				{
					SetAllExpansion(false);
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("EnableAll", "Enable All"))
				.ToolTipText(LOCTEXT("EnableAllTooltip", "Re-enable every motion component on this actor."))
				.OnClicked_Lambda([this]()
				{
					SetAllEnabled(true);
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
			.Text_Lambda([this]()
			{
				if (!Snapshot.Mesh.IsValid())
				{
					return LOCTEXT("NoSource", "Nothing is evaluating. Start play-in-editor.");
				}

				return FText::Format(
					LOCTEXT("SnapshotSummary", "{0} · aim {1} · {2} · {3} op(s)"),
					FText::FromName(Snapshot.BoneName),
					FText::AsNumber(Snapshot.AimingState),
					Snapshot.bFirstPerson ? LOCTEXT("FirstPerson", "first person") : LOCTEXT("ThirdPerson", "third person"),
					Snapshot.Ops.Num());
			})
		];
}

TSharedRef<SWidget> SACFProceduralMotionInspector::BuildCurvePanel()
{
	return SNew(SExpandableArea)
		.InitiallyCollapsed(true)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(6.0f, 4.0f))
		.HeaderContent()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CurvesTitle", "Curves"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text_Lambda([this]()
				{
					return InterestingCurves.Num() > 0
						? LOCTEXT("CurvesReferenced", "referenced by presets")
						: LOCTEXT("CurvesAll", "all pose curves");
				})
			]
		]
		.BodyContent()
		[
			SNew(SBox)
			.MaxDesiredHeight(180.0f)
			[
				SNew(SScrollBox)

				+ SScrollBox::Slot()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([this]()
					{
						TArray<FString> Lines;
						for (const TPair<FName, float>& Curve : Snapshot.Curves)
						{
							if (InterestingCurves.Num() > 0 && !InterestingCurves.Contains(Curve.Key))
							{
								continue;
							}

							Lines.Add(FString::Printf(TEXT("%s   %.3f"), *Curve.Key.ToString(), Curve.Value));
						}

						if (Lines.Num() == 0)
						{
							return LOCTEXT("NoCurves", "No mask or driver curves on the pose.");
						}

						Lines.Sort();
						return FText::FromString(FString::Join(Lines, TEXT("\n")));
					})
				]
			]
		];
}

void SACFProceduralMotionInspector::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (InCurrentTime - LastRefreshTime < ACFProceduralMotionInspector::RefreshInterval)
	{
		return;
	}

	LastRefreshTime = InCurrentTime;
	RefreshFromRegistry();
}

void SACFProceduralMotionInspector::RefreshFromRegistry()
{
	AvailableSnapshots.Reset();
	FACFProceduralMotionDebugRegistry::Get().GetSnapshots(AvailableSnapshots);

	const FACFProceduralMotionDebugSnapshot* Chosen = nullptr;

	if (SelectedMesh.IsValid())
	{
		Chosen = AvailableSnapshots.FindByPredicate([this](const FACFProceduralMotionDebugSnapshot& Candidate)
		{
			return Candidate.Mesh == SelectedMesh;
		});
	}

	if (!Chosen && AvailableSnapshots.Num() > 0)
	{
		// Prefer the first-person view, which is what people are almost always looking at.
		Chosen = AvailableSnapshots.FindByPredicate([](const FACFProceduralMotionDebugSnapshot& Candidate)
		{
			return Candidate.bFirstPerson;
		});

		if (!Chosen)
		{
			Chosen = &AvailableSnapshots[0];
		}
	}

	Snapshot = Chosen ? *Chosen : FACFProceduralMotionDebugSnapshot();

	InterestingCurves.Reset();
	for (const FACFProceduralMotionDebugComponent& Entry : Snapshot.Components)
	{
		const UACFProceduralMotionComponent* Component = Entry.Component.Get();
		if (!Component || !Component->PresetData)
		{
			continue;
		}

		for (const TObjectPtr<UACFProceduralPreset>& Preset : Component->PresetData->GetPresets())
		{
			if (!Preset)
			{
				continue;
			}

			for (TFieldIterator<FNameProperty> It(Preset->GetClass()); It; ++It)
			{
				if (It->GetName().Contains(TEXT("Curve")))
				{
					InterestingCurves.Add(*It->ContainerPtrToValuePtr<FName>(Preset));
				}
			}
		}
	}

	RebuildRows();
}

void SACFProceduralMotionInspector::RebuildRows()
{
	// Rows come from the harvested components, not from emitted ops — a muted component emits nothing,
	// and a row that vanishes when you mute it can never be un-muted.
	struct FComponentEntry
	{
		TWeakObjectPtr<UACFProceduralMotionComponent> Component;
		FString Name;
		FString OwnerName;
		/**
		 * Owner class plus component name. Unique — a character and its weapon can both own an "Offset" —
		 * and stable across play sessions and equipment swaps, which is what expansion memory needs.
		 */
		FString Id;
		int32 Stage = 0;
		TArray<const FACFProceduralMotionDebugOp*> Ops;
		const TArray<FACFProceduralMotionDebugValue>* DebugValues = nullptr;
	};

	TArray<FComponentEntry> Entries;

	for (const FACFProceduralMotionDebugComponent& SnapshotEntry : Snapshot.Components)
	{
		const TWeakObjectPtr<UACFProceduralMotionComponent>& ComponentPtr = SnapshotEntry.Component;
		const UACFProceduralMotionComponent* Component = ComponentPtr.Get();
		if (!Component)
		{
			continue;
		}

		const AActor* Owner = Component->GetOwner();

		FComponentEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Component = ComponentPtr;
		Entry.DebugValues = &SnapshotEntry.Values;
		Entry.Name = GetACFProceduralFriendlyName(Component);
		Entry.OwnerName = Owner ? Owner->GetName() : FString();
		Entry.Id = FString::Printf(TEXT("%s.%s"),
			Owner ? *Owner->GetClass()->GetName() : TEXT("None"),
			*Entry.Name);

		// Always the configured stage, never where an op happened to land — otherwise a component that
		// stops emitting jumps to a different group and the whole tree reshuffles under the cursor.
		Entry.Stage = Component->Stage;

		for (const FACFProceduralMotionDebugOp& Op : Snapshot.Ops)
		{
			if (Op.Component == ComponentPtr)
			{
				Entry.Ops.Add(&Op);
			}
		}
	}

	Entries.StableSort([](const FComponentEntry& A, const FComponentEntry& B)
	{
		return A.Stage < B.Stage;
	});

	TMap<FString, TSharedPtr<FACFProceduralInspectorRow>> ExistingByKey;

	TFunction<void(const TArray<TSharedPtr<FACFProceduralInspectorRow>>&)> Harvest =
		[&ExistingByKey, &Harvest](const TArray<TSharedPtr<FACFProceduralInspectorRow>>& Rows)
	{
		for (const TSharedPtr<FACFProceduralInspectorRow>& Row : Rows)
		{
			ExistingByKey.Add(Row->Key, Row);
			Harvest(Row->Children);
		}
	};
	Harvest(RootRows);

	auto AcquireRow = [this, &ExistingByKey](const FString& Key, EACFProceduralInspectorRowType Type) -> TSharedPtr<FACFProceduralInspectorRow>
	{
		if (const TSharedPtr<FACFProceduralInspectorRow>* Found = ExistingByKey.Find(Key))
		{
			(*Found)->Children.Reset();
			return *Found;
		}

		TSharedPtr<FACFProceduralInspectorRow> Row = MakeShared<FACFProceduralInspectorRow>();
		Row->Key = Key;

		// Stages open, components closed by default; anything the user has since toggled wins.
		if (StackTree.IsValid())
		{
			ACFProceduralTreeExpansion::ApplyRemembered(*StackTree, ExpansionByKey, Row, Type == EACFProceduralInspectorRowType::StageHeader);
		}

		return Row;
	};

	TArray<TSharedPtr<FACFProceduralInspectorRow>> NewRoots;
	TSharedPtr<FACFProceduralInspectorRow> CurrentStage;
	int32 LastStage = INDEX_NONE;

	FString Signature;

	for (const FComponentEntry& Entry : Entries)
	{
		if (!CurrentStage.IsValid() || Entry.Stage != LastStage)
		{
			const FString StageKey = FString::Printf(TEXT("S%d"), Entry.Stage);
			CurrentStage = AcquireRow(StageKey, EACFProceduralInspectorRowType::StageHeader);
			CurrentStage->Type = EACFProceduralInspectorRowType::StageHeader;
			CurrentStage->Stage = Entry.Stage;
			NewRoots.Add(CurrentStage);

			LastStage = Entry.Stage;
			Signature += StageKey;
		}

		const FString ComponentKey = FString::Printf(TEXT("C%s"), *Entry.Id);
		TSharedPtr<FACFProceduralInspectorRow> ComponentRow = AcquireRow(ComponentKey, EACFProceduralInspectorRowType::Component);
		ComponentRow->Type = EACFProceduralInspectorRowType::Component;
		ComponentRow->Stage = Entry.Stage;
		ComponentRow->Component = Entry.Component;
		ComponentRow->ComponentName = Entry.Name;
		ComponentRow->OwnerName = Entry.OwnerName;
		ComponentRow->OpCount = Entry.Ops.Num();
		CurrentStage->Children.Add(ComponentRow);

		Signature += ComponentKey;

		// Published state first: it is what explains the ops underneath it.
		if (Entry.DebugValues)
		{
			for (const FACFProceduralMotionDebugValue& Value : *Entry.DebugValues)
			{
				const FString DebugKey = FString::Printf(TEXT("D%s|%s"), *Entry.Id, *Value.Name.ToString());
				TSharedPtr<FACFProceduralInspectorRow> DebugRow = AcquireRow(DebugKey, EACFProceduralInspectorRowType::DebugValue);
				DebugRow->Type = EACFProceduralInspectorRowType::DebugValue;
				DebugRow->Stage = Entry.Stage;
				DebugRow->Component = Entry.Component;
				DebugRow->ComponentName = Entry.Name;
				DebugRow->Debug = Value;
				ComponentRow->Children.Add(DebugRow);

				Signature += DebugKey;
			}
		}

		for (int32 Index = 0; Index < Entry.Ops.Num(); ++Index)
		{
			const FString OpKey = FString::Printf(TEXT("O%s|%d"), *Entry.Id, Index);
			TSharedPtr<FACFProceduralInspectorRow> OpRow = AcquireRow(OpKey, EACFProceduralInspectorRowType::Op);
			OpRow->Type = EACFProceduralInspectorRowType::Op;
			OpRow->Stage = Entry.Ops[Index]->Stage;
			OpRow->Component = Entry.Component;
			OpRow->ComponentName = Entry.Name;
			OpRow->OwnerName = Entry.OwnerName;
			OpRow->Op = *Entry.Ops[Index];
			ComponentRow->Children.Add(OpRow);

			Signature += OpKey;
		}

		// A component that emits nothing still gets a child, so its expander arrow never appears and
		// disappears with the motion. Aiming is exactly when you cannot chase a flickering arrow.
		// Published state counts: a component with debug rows already has something to expand.
		if (ComponentRow->Children.Num() == 0)
		{
			const FString PlaceholderKey = FString::Printf(TEXT("N%s"), *Entry.Id);
			TSharedPtr<FACFProceduralInspectorRow> PlaceholderRow = AcquireRow(PlaceholderKey, EACFProceduralInspectorRowType::Placeholder);
			PlaceholderRow->Type = EACFProceduralInspectorRowType::Placeholder;
			PlaceholderRow->Stage = Entry.Stage;
			PlaceholderRow->Component = Entry.Component;
			PlaceholderRow->ComponentName = Entry.Name;
			ComponentRow->Children.Add(PlaceholderRow);

			Signature += PlaceholderKey;
		}
	}

	RootRows = MoveTemp(NewRoots);

	// Only touch the tree when its shape changed. Values update through the rows the widgets already bind to.
	if (Signature != RowSignature)
	{
		RowSignature = MoveTemp(Signature);

		if (StackTree.IsValid())
		{
			StackTree->RequestTreeRefresh();
		}
	}
}

void SACFProceduralMotionInspector::GetRowChildren(TSharedPtr<FACFProceduralInspectorRow> Row, TArray<TSharedPtr<FACFProceduralInspectorRow>>& OutChildren)
{
	if (Row.IsValid())
	{
		OutChildren = Row->Children;
	}
}

void SACFProceduralMotionInspector::HandleExpansionChanged(TSharedPtr<FACFProceduralInspectorRow> Row, bool bExpanded)
{
	ACFProceduralTreeExpansion::Remember(ExpansionByKey, Row, bExpanded);
}

TSharedPtr<SWidget> SACFProceduralMotionInspector::BuildRowContextMenu()
{
	if (!StackTree.IsValid())
	{
		return nullptr;
	}

	const TArray<TSharedPtr<FACFProceduralInspectorRow>> Selected = StackTree->GetSelectedItems();
	if (Selected.Num() == 0)
	{
		return nullptr;
	}

	return ACFProceduralTreeExpansion::BuildContextMenu(*StackTree, ExpansionByKey, Selected[0]);
}

void SACFProceduralMotionInspector::SetAllExpansion(bool bExpand)
{
	if (StackTree.IsValid())
	{
		ACFProceduralTreeExpansion::SetAll(*StackTree, ExpansionByKey, RootRows, bExpand);
	}
}

TSharedRef<SWidget> SACFProceduralMotionInspector::BuildChannelRow(const FText& Label, TSharedPtr<FACFProceduralInspectorRow> Row, bool bRotation) const
{
	using namespace ACFProceduralMotionInspector;

	auto GetChannel = [Row, bRotation](int32 Axis) -> double
	{
		const FTransform& Transform = Row->Op.Transform;

		if (!bRotation)
		{
			return Transform.GetLocation()[Axis];
		}

		const FRotator Rotation = Transform.Rotator();
		return Axis == 0 ? Rotation.Pitch : (Axis == 1 ? Rotation.Yaw : Rotation.Roll);
	};

	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);

	Box->AddSlot()
		.AutoWidth()
		.MinWidth(ChannelLabelWidth)
		[
			SNew(STextBlock)
			.Text(Label)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Box->AddSlot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([GetChannel, Axis]() { return FormatChannel(GetChannel(Axis)); })
				.ColorAndOpacity_Lambda([GetChannel, Axis]() { return GetChannelColor(GetChannel(Axis)); })
			];
	}

	return Box;
}

TSharedRef<ITableRow> SACFProceduralMotionInspector::GenerateStackRow(TSharedPtr<FACFProceduralInspectorRow> Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	using FRowType = STableRow<TSharedPtr<FACFProceduralInspectorRow>>;

	if (Row->Type == EACFProceduralInspectorRowType::StageHeader)
	{
		return SNew(FRowType, OwnerTable)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("StageHeader", "Stage {0}"), Row->Stage))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
			];
	}

	if (Row->Type == EACFProceduralInspectorRowType::Placeholder)
	{
		return SNew(FRowType, OwnerTable)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoOpsThisFrame", "no ops this frame"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}

	if (Row->Type == EACFProceduralInspectorRowType::DebugValue)
	{
		return SNew(FRowType, OwnerTable)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([Row]() { return FText::FromName(Row->Debug.Name); })
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([Row]() { return FText::FromString(Row->Debug.Value); })
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
			];
	}

	if (Row->Type == EACFProceduralInspectorRowType::Component)
	{
		return SNew(FRowType, OwnerTable)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 2.0f, 4.0f, 2.0f)
				[
					SNew(SCheckBox)
					.ToolTipText(LOCTEXT("EnabledTooltip", "Whether this component contributes to the stack."))
					.IsChecked_Lambda([Row]()
					{
						const UACFProceduralMotionComponent* Component = Row->Component.Get();
						return (Component && Component->bEnabled) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([Row](ECheckBoxState NewState)
					{
						if (UACFProceduralMotionComponent* Component = Row->Component.Get())
						{
							Component->bEnabled = NewState == ECheckBoxState::Checked;
						}
					})
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([Row]() { return FText::FromString(Row->ComponentName); })
					.ToolTipText_Lambda([Row]()
					{
						return Row->OwnerName.IsEmpty()
							? FText::FromString(Row->ComponentName)
							: FText::Format(LOCTEXT("ComponentOwner", "{0} on {1}"), FText::FromString(Row->ComponentName), FText::FromString(Row->OwnerName));
					})
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.ColorAndOpacity_Lambda([Row]()
					{
						const UACFProceduralMotionComponent* Component = Row->Component.Get();
						return (Component && Component->bEnabled) ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([Row]()
					{
						return Row->OpCount == 0
							? LOCTEXT("NoOps", "silent")
							: FText::AsNumber(Row->OpCount);
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 1.0f, 2.0f, 1.0f)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.ToolTipText(LOCTEXT("SoloTooltip", "Mute every other component. Press again to restore them all."))
					.IsChecked_Lambda([this, Row]()
					{
						return IsSoloed(Row->Component.Get()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this, Row](ECheckBoxState)
					{
						ToggleSolo(Row->Component.Get());
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SoloShort", "S"))
						.Margin(FMargin(4.0f, 0.0f))
					]
				]
			];
	}

	return SNew(FRowType, OwnerTable)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 1.0f, 4.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([Row]()
					{
						return FText::FromString(FString::Printf(
							TEXT("%s · %s"),
							ACFProceduralMotionInspector::CompositionToString(Row->Op.Composition),
							ACFProceduralMotionInspector::SpaceToString(Row->Op.Space)));
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([Row]() { return FText::FromString(FString::Printf(TEXT("w %.2f"), Row->Op.Weight)); })
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				BuildChannelRow(LOCTEXT("LocationChannel", "L"), Row, false)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 4.0f, 2.0f)
			[
				BuildChannelRow(LOCTEXT("RotationChannel", "R"), Row, true)
			]
		];
}

bool SACFProceduralMotionInspector::IsSoloed(const UACFProceduralMotionComponent* Component) const
{
	if (!Component || !Component->bEnabled)
	{
		return false;
	}

	for (const FACFProceduralMotionDebugComponent& Entry : Snapshot.Components)
	{
		const UACFProceduralMotionComponent* Other = Entry.Component.Get();
		if (Other && Other != Component && Other->bEnabled)
		{
			return false;
		}
	}

	return true;
}

void SACFProceduralMotionInspector::ToggleSolo(UACFProceduralMotionComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (IsSoloed(Component))
	{
		SetAllEnabled(true);
		return;
	}

	for (const FACFProceduralMotionDebugComponent& Entry : Snapshot.Components)
	{
		if (UACFProceduralMotionComponent* Other = Entry.Component.Get())
		{
			Other->bEnabled = Other == Component;
		}
	}
}

void SACFProceduralMotionInspector::SetAllEnabled(bool bEnabled)
{
	for (const FACFProceduralMotionDebugComponent& Entry : Snapshot.Components)
	{
		if (UACFProceduralMotionComponent* Component = Entry.Component.Get())
		{
			Component->bEnabled = bEnabled;
		}
	}
}

FText SACFProceduralMotionInspector::GetSourceText() const
{
	if (Snapshot.MeshName.IsEmpty())
	{
		return LOCTEXT("SourceNone", "No source");
	}

	return FText::FromString(Snapshot.MeshName);
}

TSharedRef<SWidget> SACFProceduralMotionInspector::BuildSourceMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SourceAuto", "Automatic"),
		LOCTEXT("SourceAutoTooltip", "Follow the first-person view, or whatever is evaluating."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]() { SelectedMesh = nullptr; })));

	for (const FACFProceduralMotionDebugSnapshot& Available : AvailableSnapshots)
	{
		TWeakObjectPtr<const USkeletalMeshComponent> Mesh = Available.Mesh;

		MenuBuilder.AddMenuEntry(
			FText::FromString(Available.MeshName),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Mesh]() { SelectedMesh = Mesh; })));
	}

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE

#endif
