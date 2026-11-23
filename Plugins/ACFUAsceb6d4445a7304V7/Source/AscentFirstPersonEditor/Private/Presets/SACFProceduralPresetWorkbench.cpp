// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Presets/SACFProceduralPresetWorkbench.h"

#include "ACFFirstPersonEditorSettings.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/Selection.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Presets/ACFProceduralPreset.h"
#include "ACFProceduralTreeExpansion.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Components/ACFProceduralMotionComponent.h"
#include "Styling/AppStyle.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ACFProceduralPresetWorkbench"

namespace ACFProceduralPresetWorkbench
{
	/** Slow enough not to fight with editing, fast enough to notice a loadout change. */
	static const double RefreshInterval = 0.5;

	static const float WideLayoutThreshold = 620.0f;

	/** Reserved for the per-binding status, so the pickers line up down the column. */
	static const float StatusColumnWidth = 56.0f;

	/** Depth-first search over the row tree. */
	static TSharedPtr<FACFProceduralPresetRow> FindRow(const TArray<TSharedPtr<FACFProceduralPresetRow>>& Rows, TFunctionRef<bool(const FACFProceduralPresetRow&)> Predicate)
	{
		for (const TSharedPtr<FACFProceduralPresetRow>& Row : Rows)
		{
			if (!Row.IsValid())
			{
				continue;
			}

			if (Predicate(*Row))
			{
				return Row;
			}

			if (TSharedPtr<FACFProceduralPresetRow> Found = FindRow(Row->Children, Predicate))
			{
				return Found;
			}
		}

		return nullptr;
	}

	/**
	 * The actor a binding belongs to.
	 *
	 * A live component answers GetOwner(); a blueprint component template has no owner, so the class
	 * default object of whichever generated class it lives in is the closest equivalent.
	 */
	static UObject* GetBindingActor(UObject* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}

		if (Owner->IsA<AActor>())
		{
			return Owner;
		}

		if (const UActorComponent* Component = Cast<UActorComponent>(Owner))
		{
			if (AActor* ComponentOwner = Component->GetOwner())
			{
				return ComponentOwner;
			}
		}

		if (UClass* OuterClass = Owner->GetTypedOuter<UClass>())
		{
			return OuterClass->GetDefaultObject();
		}

		return nullptr;
	}

	static FString GetActorLabel(UObject* Actor)
	{
		if (!Actor)
		{
			return TEXT("Unowned");
		}

		FString Name = GetACFProceduralFriendlyName(Actor);

		// A class default object reads as "Default__BP_AK_C", which helps nobody.
		Name.RemoveFromStart(TEXT("Default__"));
		Name.RemoveFromEnd(TEXT("_C"));

		return Name;
	}

	/**
	 * A native component's archetype is its template on the owner's class default object. One added at runtime, which
	 * is how the first person component adds every motion component, has its own class default object instead: shared
	 * by every instance of that class and never saved, so there is nothing worth writing to.
	 */
	static UObject* GetPersistentArchetype(UObject* Instance)
	{
		UObject* Archetype = Instance ? Instance->GetArchetype() : nullptr;
		return Archetype && !Archetype->HasAnyFlags(RF_ClassDefaultObject) ? Archetype : nullptr;
	}

	/**
	 * Where a change has to be written to outlive play.
	 *
	 * A play instance is thrown away when play stops, so writing to it looks like it worked and then
	 * silently reverts. The template it was created from is the thing that persists.
	 *
	 * The subtle case is an inherited component: a play instance of a component declared on
	 * BP_BaseEquipment has that *parent's* template as its archetype, so writing there would change every
	 * weapon deriving from it. The child needs its own override first — which is what the details panel
	 * does for you when you edit an inherited component by hand.
	 */
	static UObject* ResolveDefaultsTarget(UObject* Instance)
	{
		if (!Instance || Instance->IsTemplate())
		{
			return Instance;
		}

		UActorComponent* Component = Cast<UActorComponent>(Instance);
		AActor* OwnerActor = Component ? Component->GetOwner() : Cast<AActor>(Instance);

		if (!Component)
		{
			return OwnerActor ? OwnerActor->GetClass()->GetDefaultObject() : nullptr;
		}

		UBlueprintGeneratedClass* LeafClass = OwnerActor ? Cast<UBlueprintGeneratedClass>(OwnerActor->GetClass()) : nullptr;
		if (!LeafClass)
		{
			return GetPersistentArchetype(Instance);
		}

		const FName VariableName = Component->GetFName();

		for (UClass* Current = LeafClass; Current; Current = Current->GetSuperClass())
		{
			UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Current);
			if (!Generated || !Generated->SimpleConstructionScript)
			{
				continue;
			}

			for (USCS_Node* Node : Generated->SimpleConstructionScript->GetAllNodes())
			{
				if (!Node || Node->GetVariableName() != VariableName)
				{
					continue;
				}

				// Declared on the blueprint we are editing: its own template is the target.
				if (Generated == LeafClass)
				{
					return Node->ComponentTemplate;
				}

				const FComponentKey Key(Node);
				UInheritableComponentHandler* Handler = LeafClass->GetInheritableComponentHandler(true);
				if (!Handler)
				{
					return nullptr;
				}

				if (UActorComponent* Existing = Handler->GetOverridenComponentTemplate(Key))
				{
					return Existing;
				}

				return Handler->CreateOverridenComponentTemplate(Key);
			}
		}

		// Native component: this class's default object already holds its own copy.
		return GetPersistentArchetype(Instance);
	}

	static UBlueprint* FindOwningBlueprint(UObject* Object)
	{
		if (!Object)
		{
			return nullptr;
		}

		if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Object->GetClass()))
		{
			return Blueprint;
		}

		const UClass* OuterClass = Object->GetTypedOuter<UClass>();
		return OuterClass ? UBlueprint::GetBlueprintFromClass(OuterClass) : nullptr;
	}

	/** Sets the property on one object and tells the editor about it. */
	static void WritePresetData(UObject* Target, FObjectPropertyBase* Property, UACFProceduralPresetData* NewValue)
	{
		if (!Target || !Property)
		{
			return;
		}

		Target->SetFlags(RF_Transactional);
		Target->Modify();

		Property->SetObjectPropertyValue_InContainer(Target, NewValue);

		FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
		Target->PostEditChangeProperty(ChangedEvent);

		// A play instance has no package worth dirtying, and its blueprint is not the thing that changed.
		if (!Target->IsTemplate())
		{
			return;
		}

		if (UBlueprint* Blueprint = FindOwningBlueprint(Target))
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}

		Target->MarkPackageDirty();
	}

	static bool IsActorBlueprint(const FAssetData& AssetData)
	{
		const UClass* AssetClass = AssetData.GetClass();
		if (!AssetClass || !AssetClass->IsChildOf(UBlueprint::StaticClass()))
		{
			return false;
		}

		FString NativeParentClassPath;
		if (!AssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParentClassPath))
		{
			// Unknown parent: better to offer the asset than to hide it.
			return true;
		}

		const FSoftClassPath ParentPath(FPackageName::ExportTextPathToObjectPath(NativeParentClassPath));
		const UClass* NativeParent = ParentPath.ResolveClass();

		return !NativeParent || NativeParent->IsChildOf(AActor::StaticClass());
	}
}

void SACFProceduralPresetWorkbench::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditor.CreateDetailView(DetailsArgs);

	SAssignNew(Tree, STreeView<TSharedPtr<FACFProceduralPresetRow>>)
		.TreeItemsSource(&RootRows)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SACFProceduralPresetWorkbench::GenerateRow)
		.OnGetChildren(this, &SACFProceduralPresetWorkbench::GetRowChildren)
		.OnSelectionChanged(this, &SACFProceduralPresetWorkbench::HandleSelectionChanged)
		.OnExpansionChanged(this, &SACFProceduralPresetWorkbench::HandleExpansionChanged)
		.OnContextMenuOpening(this, &SACFProceduralPresetWorkbench::BuildRowContextMenu);

	HeaderWidget = BuildHeader();

	RebuildLayout();
	Refresh();
}

void SACFProceduralPresetWorkbench::RebuildLayout()
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 4.0f)
		[
			HeaderWidget.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(bWideLayout ? Orient_Horizontal : Orient_Vertical)

			+ SSplitter::Slot()
			.Value(0.5f)
			[
				Tree.ToSharedRef()
			]

			+ SSplitter::Slot()
			.Value(0.5f)
			[
				DetailsView.ToSharedRef()
			]
		]
	];
}

TSharedRef<SWidget> SACFProceduralPresetWorkbench::BuildHeader()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UBlueprint::StaticClass())
				.ObjectPath(this, &SACFProceduralPresetWorkbench::GetPickedTargetPath)
				.OnObjectChanged(this, &SACFProceduralPresetWorkbench::HandleTargetChanged)
				.OnShouldFilterAsset(this, &SACFProceduralPresetWorkbench::ShouldFilterAsset)
				.AllowClear(true)
				.DisplayThumbnail(false)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("UseSelection", "Use Selection"))
				.ToolTipText(LOCTEXT("UseSelectionTooltip", "Selected actor, else the selected asset."))
				.OnClicked_Lambda([this]()
				{
					UseCurrentSelection();
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
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
			.VAlign(VAlign_Center)
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
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("FollowTooltip", "While playing, show the local player's pawn and everything attached to it."))
				.IsChecked_Lambda([this]() { return bFollowPlayInEditor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bFollowPlayInEditor = NewState == ECheckBoxState::Checked;
					Refresh();
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FollowPIE", "Follow play-in-editor"))
					.Margin(FMargin(4.0f, 0.0f))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("VisibleOnlyTooltip", "While playing, leave out actors that are hidden — stowed equipment — so only what is in hand is listed. No effect outside play."))
				.IsChecked_Lambda([this]() { return bOnlyVisibleActors ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bOnlyVisibleActors = NewState == ECheckBoxState::Checked;
					Refresh();
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VisibleOnly", "Equipped only"))
					.Margin(FMargin(4.0f, 0.0f))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("WriteDefaultsTooltip", "Write preset data changes to the blueprint defaults as well as the running instance, so they survive stopping play. Off changes only the instance."))
				.IsChecked_Lambda([]()
				{
					const UACFFirstPersonEditorSettings* Settings = UACFFirstPersonEditorSettings::Get();
					return Settings && Settings->bWriteToBlueprintDefaults ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					UACFFirstPersonEditorSettings* Settings = GetMutableDefault<UACFFirstPersonEditorSettings>();
					Settings->bWriteToBlueprintDefaults = NewState == ECheckBoxState::Checked;
					Settings->SaveConfig();
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WriteDefaults", "Write to defaults"))
					.Margin(FMargin(4.0f, 0.0f))
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SACFProceduralPresetWorkbench::GetSummaryText)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		];
}

void SACFProceduralPresetWorkbench::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const bool bWide = AllottedGeometry.GetLocalSize().X > ACFProceduralPresetWorkbench::WideLayoutThreshold;
	if (bWide != bWideLayout)
	{
		bWideLayout = bWide;
		RebuildLayout();
	}

	if (InCurrentTime - LastRefreshTime < ACFProceduralPresetWorkbench::RefreshInterval)
	{
		return;
	}

	LastRefreshTime = InCurrentTime;
	Refresh();
}

UObject* SACFProceduralPresetWorkbench::ResolveEffectiveTarget() const
{
	if (bFollowPlayInEditor && GEditor)
	{
		if (const UWorld* PlayWorld = GEditor->PlayWorld)
		{
			if (const APlayerController* Controller = PlayWorld->GetFirstPlayerController())
			{
				if (APawn* Pawn = Controller->GetPawn())
				{
					return Pawn;
				}
			}
		}
	}

	return PickedTarget.Get();
}

void SACFProceduralPresetWorkbench::SetPickedTarget(UObject* Target)
{
	PickedTarget = Target;
	Refresh();
}

void SACFProceduralPresetWorkbench::Refresh()
{
	using namespace ACFProceduralPresetWorkbench;

	UObject* Target = ResolveEffectiveTarget();
	EffectiveTarget = Target;

	Context.Build(Target);

	TMap<FString, TSharedPtr<FACFProceduralPresetRow>> ExistingByKey;

	TFunction<void(const TArray<TSharedPtr<FACFProceduralPresetRow>>&)> Harvest =
		[&ExistingByKey, &Harvest](const TArray<TSharedPtr<FACFProceduralPresetRow>>& Rows)
	{
		for (const TSharedPtr<FACFProceduralPresetRow>& Row : Rows)
		{
			ExistingByKey.Add(Row->Key, Row);
			Harvest(Row->Children);
		}
	};
	Harvest(RootRows);

	auto AcquireRow = [this, &ExistingByKey](const FString& Key, bool bExpandByDefault) -> TSharedPtr<FACFProceduralPresetRow>
	{
		if (const TSharedPtr<FACFProceduralPresetRow>* Found = ExistingByKey.Find(Key))
		{
			(*Found)->Children.Reset();
			return *Found;
		}

		TSharedPtr<FACFProceduralPresetRow> Row = MakeShared<FACFProceduralPresetRow>();
		Row->Key = Key;

		if (Tree.IsValid())
		{
			ACFProceduralTreeExpansion::ApplyRemembered(*Tree, ExpansionByKey, Row, bExpandByDefault);
		}

		return Row;
	};

	FString Signature;

	// ---- Bindings, grouped by the actor that owns them ----
	TSharedPtr<FACFProceduralPresetRow> BindingsSection = AcquireRow(TEXT("SEC_Bindings"), true);
	BindingsSection->Type = EACFProceduralPresetRowType::Section;
	BindingsSection->Label = LOCTEXT("BindingsSection", "Bindings");
	Signature += TEXT("SEC_Bindings");

	TArray<UObject*> ActorOrder;
	TMap<UObject*, TArray<const FACFProceduralPresetBinding*>> ByActor;

	// Stowed equipment is hidden, not destroyed, so every weapon in the loadout is reachable at once.
	// Filtering on visibility narrows the list to what is actually in hand. Only meaningful while playing.
	const bool bFilterHidden = bOnlyVisibleActors && Context.bIsLiveActor;

	HiddenBindingCount = 0;

	for (const FACFProceduralPresetBinding& Binding : Context.PresetBindings)
	{
		UObject* Actor = GetBindingActor(Binding.Owner.Get());

		if (bFilterHidden)
		{
			const AActor* AsActor = Cast<AActor>(Actor);
			if (AsActor && AsActor->IsHidden())
			{
				++HiddenBindingCount;
				continue;
			}
		}

		if (!ByActor.Contains(Actor))
		{
			ActorOrder.Add(Actor);
		}

		ByActor.FindOrAdd(Actor).Add(&Binding);
	}

	// Presets are listed for the assets the surviving bindings actually reference, so both halves of the
	// tree agree about what is in scope.
	TArray<UACFProceduralPresetData*> ReferencedPresetData;

	for (UObject* Actor : ActorOrder)
	{
		const TArray<const FACFProceduralPresetBinding*>& ActorBindings = ByActor[Actor];

		// Class name, not object id: the same key then identifies BP_AK in the editor and its play instance,
		// so expansion carries across starting and stopping play.
		const FString ActorKey = FString::Printf(TEXT("A%s"), Actor ? *Actor->GetClass()->GetName() : TEXT("None"));
		TSharedPtr<FACFProceduralPresetRow> ActorRow = AcquireRow(ActorKey, true);
		ActorRow->Type = EACFProceduralPresetRowType::Actor;
		ActorRow->Label = FText::FromString(GetActorLabel(Actor));
		BindingsSection->Children.Add(ActorRow);
		Signature += ActorKey;

		// Majority is computed per actor: a character and its weapons legitimately use different assets,
		// so comparing across the whole target flags almost everything.
		UACFProceduralPresetData* Majority = nullptr;
		{
			TMap<UACFProceduralPresetData*, int32> Counts;
			for (const FACFProceduralPresetBinding* Binding : ActorBindings)
			{
				if (UACFProceduralPresetData* Value = Binding->PresetData.Get())
				{
					Counts.FindOrAdd(Value)++;
				}
			}

			int32 Best = 0;
			for (const TPair<UACFProceduralPresetData*, int32>& Pair : Counts)
			{
				if (Pair.Value > Best)
				{
					Best = Pair.Value;
					Majority = Pair.Key;
				}
			}
		}

		for (const FACFProceduralPresetBinding* Binding : ActorBindings)
		{
			UObject* Owner = Binding->Owner.Get();

			const FString BindingKey = FString::Printf(TEXT("B%s.%s|%s"),
				*ActorKey,
				Owner ? *GetACFProceduralFriendlyName(Owner) : TEXT("None"),
				Binding->Property ? *Binding->Property->GetName() : TEXT("None"));

			TSharedPtr<FACFProceduralPresetRow> BindingRow = AcquireRow(BindingKey, false);
			BindingRow->Type = EACFProceduralPresetRowType::Binding;
			BindingRow->Binding = *Binding;
			BindingRow->PresetData = Binding->PresetData;

			const bool bIsActor = Owner && Owner->IsA<AActor>();
			BindingRow->Label = bIsActor
				? LOCTEXT("SelfBinding", "Self")
				: FText::FromString(GetACFProceduralFriendlyName(Owner));

			UACFProceduralPresetData* Value = Binding->PresetData.Get();
			BindingRow->bDiffers = Value && Majority && Value != Majority;

			if (Value)
			{
				ReferencedPresetData.AddUnique(Value);
			}

			ActorRow->Children.Add(BindingRow);
			Signature += BindingKey;
		}
	}

	// ---- Presets, grouped by the asset they live in ----
	TSharedPtr<FACFProceduralPresetRow> PresetsSection = AcquireRow(TEXT("SEC_Presets"), true);
	PresetsSection->Type = EACFProceduralPresetRowType::Section;
	PresetsSection->Label = LOCTEXT("PresetsSection", "Presets");
	Signature += TEXT("SEC_Presets");

	for (UACFProceduralPresetData* PresetData : ReferencedPresetData)
	{
		const FString DataKey = FString::Printf(TEXT("D%s"), *PresetData->GetName());
		TSharedPtr<FACFProceduralPresetRow> DataRow = AcquireRow(DataKey, true);
		DataRow->Type = EACFProceduralPresetRowType::PresetData;
		DataRow->Label = FText::FromString(PresetData->GetName());
		DataRow->PresetData = PresetData;
		PresetsSection->Children.Add(DataRow);
		Signature += DataKey;

		for (const TObjectPtr<UACFProceduralPreset>& Preset : PresetData->GetPresets())
		{
			if (!Preset)
			{
				continue;
			}

			const FString PresetKey = FString::Printf(TEXT("P%s.%s"), *DataKey, *Preset->GetClass()->GetName());
			TSharedPtr<FACFProceduralPresetRow> PresetRow = AcquireRow(PresetKey, false);
			PresetRow->Type = EACFProceduralPresetRowType::Preset;
			PresetRow->Label = FText::FromString(Preset->GetPresetName());
			PresetRow->Preset = Preset;
			PresetRow->PresetData = PresetData;
			DataRow->Children.Add(PresetRow);
			Signature += PresetKey;
		}

		if (DataRow->Children.Num() == 0)
		{
			const FString EmptyKey = FString::Printf(TEXT("N%s"), *DataKey);
			TSharedPtr<FACFProceduralPresetRow> EmptyRow = AcquireRow(EmptyKey, false);
			EmptyRow->Type = EACFProceduralPresetRowType::Placeholder;
			EmptyRow->Label = LOCTEXT("NoPresets", "no presets in this asset");
			DataRow->Children.Add(EmptyRow);
			Signature += EmptyKey;
		}
	}

	ShownBindingCount = Context.PresetBindings.Num() - HiddenBindingCount;
	ShownPresetDataCount = ReferencedPresetData.Num();

	RootRows.Reset();
	RootRows.Add(PresetsSection);
	RootRows.Add(BindingsSection);

	if (Signature != RowSignature)
	{
		RowSignature = MoveTemp(Signature);

		if (Tree.IsValid())
		{
			Tree->RequestTreeRefresh();
		}
	}

	RestoreSelection();
}

void SACFProceduralPresetWorkbench::GetRowChildren(TSharedPtr<FACFProceduralPresetRow> Row, TArray<TSharedPtr<FACFProceduralPresetRow>>& OutChildren)
{
	if (Row.IsValid())
	{
		OutChildren = Row->Children;
	}
}

void SACFProceduralPresetWorkbench::HandleExpansionChanged(TSharedPtr<FACFProceduralPresetRow> Row, bool bExpanded)
{
	ACFProceduralTreeExpansion::Remember(ExpansionByKey, Row, bExpanded);
}

TSharedPtr<SWidget> SACFProceduralPresetWorkbench::BuildRowContextMenu()
{
	if (!Tree.IsValid())
	{
		return nullptr;
	}

	const TArray<TSharedPtr<FACFProceduralPresetRow>> Selected = Tree->GetSelectedItems();
	if (Selected.Num() == 0)
	{
		return nullptr;
	}

	return ACFProceduralTreeExpansion::BuildContextMenu(*Tree, ExpansionByKey, Selected[0]);
}

void SACFProceduralPresetWorkbench::SetAllExpansion(bool bExpand)
{
	if (Tree.IsValid())
	{
		ACFProceduralTreeExpansion::SetAll(*Tree, ExpansionByKey, RootRows, bExpand);
	}
}

TSharedRef<ITableRow> SACFProceduralPresetWorkbench::GenerateRow(TSharedPtr<FACFProceduralPresetRow> Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	using FRowType = STableRow<TSharedPtr<FACFProceduralPresetRow>>;

	if (Row->Type == EACFProceduralPresetRowType::Binding)
	{
		return SNew(FRowType, OwnerTable)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.45f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 2.0f, 4.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(Row->Label)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.ToolTipText(Row->Binding.GetDisplayName())
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.55f)
				.VAlign(VAlign_Center)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UACFProceduralPresetData::StaticClass())
					.ObjectPath_Lambda([Row]()
					{
						const UACFProceduralPresetData* Value = Row->Binding.PresetData.Get();
						return Value ? Value->GetPathName() : FString();
					})
					.OnObjectChanged_Lambda([this, Row](const FAssetData& AssetData)
					{
						ApplyBinding(Row, Cast<UACFProceduralPresetData>(AssetData.GetAsset()));
					})
					.AllowClear(true)
					.DisplayThumbnail(false)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f, 2.0f, 0.0f)
				[
					// Fixed width: an auto-sized status shifts every picker by however long its text is,
					// so no two rows line up.
					SNew(SBox)
					.WidthOverride(ACFProceduralPresetWorkbench::StatusColumnWidth)
					[
						SNew(STextBlock)
						.Text_Lambda([Row]()
						{
							if (!Row->Binding.PresetData.IsValid())
							{
								const UACFProceduralMotionComponent* Component = Cast<UACFProceduralMotionComponent>(Row->Binding.Owner.Get());
								return Component && Component->GetEffectivePresetData()
									? LOCTEXT("Inherited", "inherited")
									: LOCTEXT("Unset", "unset");
							}

							return Row->bDiffers ? LOCTEXT("Differs", "differs") : FText::GetEmpty();
						})
						.ToolTipText(LOCTEXT("StatusTooltip", "\"Inherited\" means the component has no preset data of its own and uses its owning actor's. Unset is legal — the component uses its preset class defaults. \"Differs\" means it points somewhere other than the rest of this actor."))
						.ColorAndOpacity_Lambda([Row]()
						{
							return Row->bDiffers
								? FSlateColor(FLinearColor(1.0f, 0.75f, 0.25f))
								: FSlateColor::UseSubduedForeground();
						})
					]
				]
			];
	}

	const bool bIsHeading = Row->Type == EACFProceduralPresetRowType::Section;
	const bool bIsPlaceholder = Row->Type == EACFProceduralPresetRowType::Placeholder;

	TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);

	Content->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Row->Label)
			.Font(bIsHeading ? FAppStyle::GetFontStyle("PropertyWindow.BoldFont") : FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			.ColorAndOpacity(bIsPlaceholder ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		];

	if (Row->Type == EACFProceduralPresetRowType::PresetData)
	{
		Content->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 1.0f, 2.0f, 1.0f)
			[
				SNew(SButton)
				.Text_Lambda([this, Row]()
				{
					return FText::Format(
						LOCTEXT("AddMissingCount", "Add {0} missing"),
						GetMissingPresetClasses(Row->PresetData.Get()).Num());
				})
				.ToolTipText(LOCTEXT("AddMissingTooltip", "Add a preset for every component that reads this asset but finds nothing here."))
				.Visibility_Lambda([this, Row]()
				{
					return GetMissingPresetClasses(Row->PresetData.Get()).Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked_Lambda([this, Row]()
				{
					AddMissingPresets(Row->PresetData.Get());
					return FReply::Handled();
				})
			];
	}

	return SNew(FRowType, OwnerTable)
		[
			Content
		];
}

void SACFProceduralPresetWorkbench::HandleSelectionChanged(TSharedPtr<FACFProceduralPresetRow> Row, ESelectInfo::Type SelectInfo)
{
	// Direct means the tree moved the selection itself; only a real pick should change what is remembered.
	if (SelectInfo == ESelectInfo::Direct || !Row.IsValid())
	{
		return;
	}

	SelectedRowKey = Row->Key;

	// Only a preset can be followed onto other equipment; anything else drops the intent.
	const UACFProceduralPreset* Preset = Row->Type == EACFProceduralPresetRowType::Preset ? Row->Preset.Get() : nullptr;
	SelectedPresetClass = Preset ? Preset->GetClass() : nullptr;

	ShowRowInDetails(Row);
}

void SACFProceduralPresetWorkbench::ShowRowInDetails(const TSharedPtr<FACFProceduralPresetRow>& Row)
{
	if (!DetailsView.IsValid() || !Row.IsValid())
	{
		return;
	}

	UObject* ToShow = nullptr;

	switch (Row->Type)
	{
	case EACFProceduralPresetRowType::Preset:
		ToShow = Row->Preset.Get();
		break;

	case EACFProceduralPresetRowType::PresetData:
		ToShow = Row->PresetData.Get();
		break;

	case EACFProceduralPresetRowType::Binding:
		// Only ever the asset it points at. Falling back to the owning component dumped a whole actor into
		// the pane, which reads as the panel having broken rather than as "nothing is bound here".
		ToShow = Row->Binding.PresetData.Get();
		break;

	default:
		return;
	}

	if (!ToShow || DetailsObject.Get() == ToShow)
	{
		return;
	}

	DetailsObject = ToShow;
	DetailsView->SetObject(ToShow);
}

void SACFProceduralPresetWorkbench::RestoreSelection()
{
	using namespace ACFProceduralPresetWorkbench;

	if (!Tree.IsValid())
	{
		return;
	}

	TSharedPtr<FACFProceduralPresetRow> Match;

	if (!SelectedRowKey.IsEmpty())
	{
		Match = FindRow(RootRows, [this](const FACFProceduralPresetRow& Row)
		{
			return Row.Key == SelectedRowKey;
		});
	}

	// The row is gone — a swap, or "Equipped only" hiding it. Follow the same preset onto what is here now.
	if (!Match.IsValid() && SelectedPresetClass.IsValid())
	{
		const UClass* WantedClass = SelectedPresetClass.Get();

		Match = FindRow(RootRows, [WantedClass](const FACFProceduralPresetRow& Row)
		{
			const UACFProceduralPreset* Preset = Row.Type == EACFProceduralPresetRowType::Preset ? Row.Preset.Get() : nullptr;
			return Preset && Preset->GetClass() == WantedClass;
		});

		if (Match.IsValid())
		{
			SelectedRowKey = Match->Key;
		}
	}

	if (Match.IsValid())
	{
		const TArray<TSharedPtr<FACFProceduralPresetRow>> Current = Tree->GetSelectedItems();
		if (Current.Num() != 1 || Current[0] != Match)
		{
			Tree->SetSelection(Match, ESelectInfo::Direct);
		}

		ShowRowInDetails(Match);
		return;
	}

	// Nothing equivalent is listed, so stop showing the old one. SelectedPresetClass is kept: swapping
	// back to equipment that has it picks the selection up again.
	if (DetailsObject.IsValid())
	{
		Tree->ClearSelection();
		DetailsObject = nullptr;
		DetailsView->SetObject(nullptr);
	}
}

void SACFProceduralPresetWorkbench::UseCurrentSelection()
{
	if (GEditor)
	{
		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			if (AActor* Actor = Selection->GetTop<AActor>())
			{
				SetPickedTarget(Actor);
				return;
			}
		}
	}

	const FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<FAssetData> SelectedAssets;
	ContentBrowser.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.Num() > 0)
	{
		SetPickedTarget(SelectedAssets[0].GetAsset());
	}
}

FString SACFProceduralPresetWorkbench::GetPickedTargetPath() const
{
	const UObject* TargetObject = PickedTarget.Get();
	return TargetObject ? TargetObject->GetPathName() : FString();
}

void SACFProceduralPresetWorkbench::HandleTargetChanged(const FAssetData& AssetData)
{
	SetPickedTarget(AssetData.GetAsset());
}

bool SACFProceduralPresetWorkbench::ShouldFilterAsset(const FAssetData& AssetData) const
{
	return !ACFProceduralPresetWorkbench::IsActorBlueprint(AssetData);
}

TArray<TSubclassOf<UACFProceduralPreset>> SACFProceduralPresetWorkbench::GetMissingPresetClasses(UACFProceduralPresetData* PresetData) const
{
	TArray<TSubclassOf<UACFProceduralPreset>> Missing;

	if (!PresetData)
	{
		return Missing;
	}

	// GetPresetClass is what makes this possible — the component-to-preset pairing used to live only
	// inside each component's .cpp, where nothing could read it.
	for (const TWeakObjectPtr<UACFProceduralMotionComponent>& ComponentPtr : Context.MotionComponents)
	{
		const UACFProceduralMotionComponent* Component = ComponentPtr.Get();
		if (!Component || Component->GetEffectivePresetData() != PresetData)
		{
			continue;
		}

		const TSubclassOf<UACFProceduralPreset> PresetClass = Component->GetPresetClass();
		if (PresetClass && !PresetData->GetPresetByClass(PresetClass))
		{
			Missing.AddUnique(PresetClass);
		}
	}

	return Missing;
}

void SACFProceduralPresetWorkbench::AddMissingPresets(UACFProceduralPresetData* PresetData)
{
	const TArray<TSubclassOf<UACFProceduralPreset>> Missing = GetMissingPresetClasses(PresetData);
	if (Missing.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddMissingPresets", "Add Missing Procedural Presets"));

	for (const TSubclassOf<UACFProceduralPreset>& PresetClass : Missing)
	{
		PresetData->AddPresetOfClass(PresetClass);
	}

	Refresh();
}

void SACFProceduralPresetWorkbench::ApplyBinding(TSharedPtr<FACFProceduralPresetRow> Row, UACFProceduralPresetData* NewValue)
{
	if (!Row.IsValid())
	{
		return;
	}

	using namespace ACFProceduralPresetWorkbench;

	UObject* Owner = Row->Binding.Owner.Get();
	FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(Row->Binding.Property);
	if (!Owner || !Property)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetPresetData", "Set Procedural Preset Data"));

	// Always the object in front of the user, so the change is visible immediately while playing.
	WritePresetData(Owner, Property, NewValue);

	const UACFFirstPersonEditorSettings* Settings = UACFFirstPersonEditorSettings::Get();
	if (Settings && Settings->bWriteToBlueprintDefaults)
	{
		UObject* DefaultsTarget = ResolveDefaultsTarget(Owner);
		if (DefaultsTarget && DefaultsTarget != Owner)
		{
			WritePresetData(DefaultsTarget, Property, NewValue);
		}
	}

	Refresh();
}

FText SACFProceduralPresetWorkbench::GetSummaryText() const
{
	if (!EffectiveTarget.IsValid())
	{
		return LOCTEXT("SummaryNone", "Pick a character or equipment blueprint, or select an actor and press Use Selection.");
	}

	const FText HiddenNote = HiddenBindingCount > 0
		? FText::Format(LOCTEXT("HiddenNote", " {0} hidden by \"Equipped only\"."), HiddenBindingCount)
		: FText::GetEmpty();

	const FText LiveNote = Context.bIsLiveActor
		? LOCTEXT("LiveHint", " Preset data is a shared asset, so edits made now survive stopping play — save the asset to keep them.")
		: FText::GetEmpty();

	return FText::Format(
		LOCTEXT("Summary", "{0} · {1} binding(s) · {2} preset asset(s).{3}{4}"),
		FText::FromString(ACFProceduralPresetWorkbench::GetActorLabel(EffectiveTarget.Get())),
		ShownBindingCount,
		ShownPresetDataCount,
		HiddenNote,
		LiveNote);
}

#undef LOCTEXT_NAMESPACE
