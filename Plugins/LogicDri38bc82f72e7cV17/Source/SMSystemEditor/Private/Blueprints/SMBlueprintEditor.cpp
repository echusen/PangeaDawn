// Copyright Recursoft LLC. All Rights Reserved.

#include "SMBlueprintEditor.h"

#include "Commands/SMEditorCommands.h"
#include "Configuration/SMEditorSettings.h"
#include "Configuration/SMProjectEditorSettings.h"
#include "Construction/ISMEditorConstructionManager.h"
#include "Graph/Nodes/PropertyNodes/SMGraphK2Node_PropertyNode_Base.h"
#include "Graph/Nodes/SMGraphK2Node_StateMachineNode.h"
#include "Graph/Nodes/SMGraphNode_AnyStateNode.h"
#include "Graph/Nodes/SMGraphNode_ConduitNode.h"
#include "Graph/Nodes/SMGraphNode_LinkStateNode.h"
#include "Graph/Nodes/SMGraphNode_RerouteNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineParentNode.h"
#include "Graph/Nodes/SMGraphNode_StateMachineStateNode.h"
#include "Graph/Nodes/SMGraphNode_StateNode.h"
#include "Graph/Nodes/SMGraphNode_TransitionEdge.h"
#include "Graph/Schema/SMGraphSchema.h"
#include "Graph/SMConduitGraph.h"
#include "Graph/SMGraphK2.h"
#include "Graph/SMIntermediateGraph.h"
#include "Graph/SMPropertyGraph.h"
#include "Graph/SMStateGraph.h"
#include "Graph/SMTransitionGraph.h"
#include "SBlueprintEditorToolbar.h"
#include "SMBlueprintEditorModes.h"
#include "UI/SMBlueprintEditorToolbar.h"
#include "Utilities/SMBlueprintEditorUtils.h"
#include "Utilities/SMEditorAuthentication.h"
#include "Utilities/SMVersionUtils.h"

#include "ISMPreviewEditorModule.h"
#include "ISMPreviewModeViewportClient.h"

#include "Blueprints/SMBlueprint.h"

#include "ContentBrowserModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "IContentBrowserSingleton.h"
#include "Kismet2/DebuggerCommands.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "SKismetInspector.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SMBlueprintEditor"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
// ^ Needed for OnSelectedNodesChangedEvent
FSMStateMachineBlueprintEditor::FSMStateMachineBlueprintEditor() :
	SelectedPropertyNode(nullptr),
	bJumpingToHyperLink(false),
	bShuttingDown(false),
	bPreviewModeAllowed(false)
{
}

FSMStateMachineBlueprintEditor::~FSMStateMachineBlueprintEditor()
{
	bShuttingDown = true;
	if (LoadedBlueprint.IsValid())
	{
		StopPreviewSimulation();
		
		if (OnDebugObjectSetHandle.IsValid())
		{
			LoadedBlueprint->OnSetObjectBeingDebugged().Remove(OnDebugObjectSetHandle);
		}
	}
	
	bShuttingDown = false;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FSMStateMachineBlueprintEditor::InitSMBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USMBlueprint* Blueprint)
{
	TSharedPtr<FSMStateMachineBlueprintEditor> Editor(SharedThis(this));
	
	// Blueprint is already current unless it was saved by a newer version of the plugin.
	FSMVersionUtils::SetToLatestVersion(Blueprint);

	bPreviewModeAllowed = FSMBlueprintEditorUtils::GetProjectEditorSettings()->bEnablePreviewMode;
	
	if (bPreviewModeAllowed)
	{
		// Recreate the preview object only if it already exists. This will clear out the previous undo/redo stack
		// which prevents odd behavior in the event previous history deletes or restores a preview actor.
		Blueprint->RecreatePreviewObject();
	}
	
	LoadedBlueprint = MakeWeakObjectPtr(Blueprint);
	
	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShared<FBlueprintEditorToolbar>(Editor);
	}

	if (!StateMachineToolbar.IsValid())
	{
		StateMachineToolbar = MakeShared<FSMBlueprintEditorToolbar>(Editor);
	}

	// So the play bar matches the level bar.
	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

	CreateDefaultCommands();
	
	// Register default and custom commands.
	BindCommands();

	RegisterMenus();
	
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, FSMBlueprintEditorModes::SMEditorName, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Blueprint, false);

	TArray<UBlueprint*> Blueprints;
	Blueprints.Add(Blueprint);

	CommonInitialization(Blueprints, false);

	const TSharedRef<FApplicationMode> GraphMode = MakeShared<FSMBlueprintEditorGraphMode>(Editor);
	AddApplicationMode(GraphMode->GetModeName(), GraphMode);
	
	if (bPreviewModeAllowed)
	{
		const TSharedRef<FApplicationMode> PreviewMode = MakeShared<FSMBlueprintEditorPreviewMode>(Editor);
		AddApplicationMode(PreviewMode->GetModeName(), PreviewMode);
	}
	
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// This does the actual layout generation.
	SetCurrentMode(GraphMode->GetModeName());

	PostLayoutBlueprintEditorInitialization();

	OnDebugObjectSetHandle = Blueprint->OnSetObjectBeingDebugged().AddRaw(this, &FSMStateMachineBlueprintEditor::OnDebugObjectSet);
}

void FSMStateMachineBlueprintEditor::CreateDefaultCommands()
{
	FBlueprintEditor::CreateDefaultCommands();

	// Might extend.
}

void FSMStateMachineBlueprintEditor::RefreshEditors(ERefreshBlueprintEditorReason::Type Reason)
{
	CloseInvalidTabs();
	FBlueprintEditor::RefreshEditors(Reason);
}

void FSMStateMachineBlueprintEditor::SetCurrentMode(FName NewMode)
{
	if (bJumpingToHyperLink)
	{
		/*
		 * For now all modes support displaying graphs and we don't want to call SetCurrentMode
		 * when jumping because default behavior sets the mode back to blueprint defaults.
		 */
		return;
	}

	if (NewMode != FSMBlueprintEditorModes::SMPreviewMode)
	{
		// Close preview session.
		StopPreviewSimulation();
	}
	
	{
		// Save current selection and restore after a mode change.
		
		FGraphPanelSelectionSet CurrentSelection = GetSelectedNodes();

		FBlueprintEditor::SetCurrentMode(NewMode);

		TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
		if (FocusedGraphEd.IsValid())
		{
			for (UObject* Selection : CurrentSelection)
			{
				if (UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(Selection))
				{
					FocusedGraphEd->SetNodeSelection(SelectedNode, true);
				}
			}
		}
	}
}

void FSMStateMachineBlueprintEditor::JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename)
{
	bJumpingToHyperLink = true;
	FBlueprintEditor::JumpToHyperlink(ObjectReference, bRequestRename);
	bJumpingToHyperLink = false;
}

void FSMStateMachineBlueprintEditor::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled)
{
	StopPreviewSimulation();
	FBlueprintEditor::OnBlueprintChangedImpl(InBlueprint, bIsJustBeingCompiled);

	// Notify any state machines that reference this state machine they need to be recompiled.
	// This is needed for Guid calculation since the owning SM will store all SM reference PathGuids.
	// Note this isn't perfect as nested references with a depth greater than one won't update all owners.
	// In this case the calculation will be performed at run-time and a warning will be logged. Compile time Guid
	// calculation should resolve itself on reload/recompile of owners and during packaging.
	const USMProjectEditorSettings* Settings = FSMBlueprintEditorUtils::GetProjectEditorSettings();
	if (Settings->bCalculateGuidsOnCompile)
	{
		TArray<UBlueprint*> Blueprints;

		FBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(InBlueprint);
		FSMBlueprintEditorUtils::GetDependentBlueprints(InBlueprint, Blueprints);

		for (UBlueprint* Blueprint : Blueprints)
		{
			// Skip if this is a child blueprint since those are already caught by the compile process.
			if (Blueprint->IsA<USMBlueprint>() && !Blueprint->bIsRegeneratingOnLoad &&
				(Blueprint->GeneratedClass && !Blueprint->GeneratedClass->IsChildOf(InBlueprint->GeneratedClass)))
			{
				// First check for a circular dependency where this blueprint is also dependent on its dependent.
				// In that case we just want to continue because UE will handle this. Otherwise the compile fails with little information.
				TArray<UBlueprint*> OtherBlueprints;
				FSMBlueprintEditorUtils::GetDependentBlueprints(Blueprint, OtherBlueprints);
				if (OtherBlueprints.Contains(InBlueprint))
				{
					continue;
				}

				Blueprint->Status = BS_Dirty;
			}
		}
	}
}

FGraphAppearanceInfo FSMStateMachineBlueprintEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = FBlueprintEditor::GetGraphAppearance(InGraph);

	if (GetStateMachineBlueprint())
	{
		LD::EditorAuth::LDAuthStatus AuthenticationStatus = LD::EditorAuth::GetUserMarketplaceAuthenticationStatus();

		if (AuthenticationStatus == LD::EditorAuth::LDAuthStatus::Succeeded || FSMAuthenticator::Get().IsAuthenticated())
		{
			AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_StateMachine", "LOGIC DRIVER");
		}
		else
		{
			AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_StateMachineInvalid", "LOGIC DRIVER (INVALID LICENSE)");
		}
		if (const UClass* GraphClass = InGraph ? InGraph->GetClass() : nullptr)
		{
			if (GraphClass->IsChildOf<USMPropertyGraph>())
			{
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Property", "PROPERTY");
			}
			else if (GraphClass->IsChildOf<USMIntermediateGraph>())
			{
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Intermediate", "REFERENCE");
			}
			else if (GraphClass->IsChildOf<USMStateGraph>())
			{
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_State", "STATE");
			}
			else if (GraphClass->IsChildOf<USMTransitionGraph>())
			{
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Transition", "TRANSITION");
			}
			else if (GraphClass->IsChildOf<USMConduitGraph>())
			{
				AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Conduit", "CONDUIT");
			}
		}
	}

	return MoveTemp(AppearanceInfo);
}

void FSMStateMachineBlueprintEditor::PasteNodesHere(UEdGraph* Graph, const FVector2f& Location)
{
	FSMBlueprintEditorUtils::FBulkCacheInvalidation CacheInvalidator(GetStateMachineBlueprint());
	FBlueprintEditor::PasteNodesHere(Graph, Location);
}

void FSMStateMachineBlueprintEditor::DeleteSelectedNodes()
{
	FSMBlueprintEditorUtils::FBulkCacheInvalidation CacheInvalidator(GetStateMachineBlueprint());
	FBlueprintEditor::DeleteSelectedNodes();
}

USMBlueprint* FSMStateMachineBlueprintEditor::GetStateMachineBlueprint() const
{
	return Cast<USMBlueprint>(LoadedBlueprint.Get());
}

bool FSMStateMachineBlueprintEditor::IsSelectedPropertyNodeValid(bool bCheckReadOnlyStatus) const
{
	if (!SelectedPropertyNode.IsValid())
	{
		return false;
	}

	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		if (const USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(Node))
		{
			const FSMGraphProperty_Base* PropertyNode = SelectedPropertyNode->GetPropertyNodeChecked();
			return (!bCheckReadOnlyStatus || !PropertyNode->IsVariableReadOnly()) && GraphNode->GetGraphPropertyNode(PropertyNode->GetGuid()) != nullptr;
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::SelectNodes(const TSet<UEdGraphNode*>& InGraphNodes, bool bZoomToFit)
{
	const TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->ClearSelectionSet();

		for (UObject* Selection : InGraphNodes)
		{
			if (UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(Selection))
			{
				FocusedGraphEd->SetNodeSelection(SelectedNode, true);
			}
		}

		if (bZoomToFit)
		{
			FocusedGraphEd->ZoomToFit(true);
		}
	}
}

void FSMStateMachineBlueprintEditor::CloseInvalidTabs()
{
	/*
	 * HACK and work around for UE4 crashing on the second time you undo the creation of a graph while the tab is open.
	 *
	 * We check if a graph is problematic and manually close the tab.
	 *
	 * The problem affects animation state machines as well. Isolating the fix here rather than in separate undo events so
	 * if the engine is ever patched we can just remove it here.
	 *
	 * Steps to reproduce:
	 *
	 * 1. Copy and paste a state
	 * 2. Open the state
	 * 3. Edit undo
	 * WORKS!
	 *
	 * 4. Paste the state again
	 * 5. Open the state
	 * 6. Edit undo
	 * CRASH!
	 */

	for (TSharedPtr<SDockTab> Tab : DocumentManager->GetAllDocumentTabs())
	{
		const TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
		UEdGraph* CurrentGraph = GraphEditor->GetCurrentGraph();
		if (CurrentGraph == nullptr || CurrentGraph->HasAnyFlags(RF_Transient))
		{
			Tab->RequestCloseTab();
			continue;
		}

		if (CurrentGraph->GetSchema() == nullptr)
		{
			// Schema is gone after an undo... can't just close it. Interior methods still look for a schema.
			CurrentGraph->Schema = USMGraphSchema::StaticClass();
			Tab->RequestCloseTab();
			continue;
		}

		// The above close doesn't like to work but the set schema fixes a crash. What we're left is
		// a tab with a null graph. Luckily we can verify the problem by checking the owning node's bound graph.
		if (USMGraphNode_Base* Node = Cast<USMGraphNode_Base>(CurrentGraph->GetOuter()))
		{
			// This can also be null on 4.21 during normal deletion!
			if (Node->GetBoundGraph() == nullptr)
			{
				Tab->RequestCloseTab();
			}
		}
		// State machine definitions also risk crashing if the editor is open when you delete them by node. Found on 4.21.
		else if (USMGraphK2Node_StateMachineNode* RootNode = Cast<USMGraphK2Node_StateMachineNode>(CurrentGraph->GetOuter()))
		{
			if (RootNode->GetStateMachineGraph() == nullptr)
			{
				Tab->RequestCloseTab();
			}
		}
	}
}

void FSMStateMachineBlueprintEditor::PostUndo(bool bSuccess)
{
	FBlueprintEditor::PostUndo(bSuccess);
	FSMBlueprintEditorUtils::FCacheInvalidationArgs Args;
	Args.bAllowIfTransacting = true;
	FSMBlueprintEditorUtils::InvalidateCaches(LoadedBlueprint.Get(), MoveTemp(Args));
	if (!bSuccess)
	{
		return;
	}

	USMBlueprint* Blueprint = GetStateMachineBlueprint();
	check(Blueprint);

	// Collapsed Graphs can be problematic if a state or transition is deleted which contains a nested graph, and then the user undoes that action.
	// The graph will be present in the graph tree, but the node itself will say "Invalid Graph". This happens whether deleting either the state node or graph.
	// It works fine if deleting the entire state machine and undoing it. There is probably a better way of doing this. TODO: Collapsed Graph revamp.
	{
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);

		USMGraphK2* FoundGraph;
		Graphs.FindItemByClass<USMGraphK2>(&FoundGraph);
		ensure(FoundGraph);

		FSMBlueprintEditorUtils::FixUpCollapsedGraphs(FSMBlueprintEditorUtils::GetTopLevelGraph(FoundGraph));
	}

	const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
	if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Standard)
	{
		ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprint(Blueprint);
	}
}

void FSMStateMachineBlueprintEditor::PostRedo(bool bSuccess)
{
	FBlueprintEditor::PostRedo(bSuccess);
	FSMBlueprintEditorUtils::FCacheInvalidationArgs Args;
	Args.bAllowIfTransacting = true;
	FSMBlueprintEditorUtils::InvalidateCaches(LoadedBlueprint.Get(), MoveTemp(Args));

	const ESMEditorConstructionScriptProjectSetting ConstructionProjectSetting = FSMBlueprintEditorUtils::GetProjectEditorSettings()->EditorNodeConstructionScriptSetting;
	if (ConstructionProjectSetting == ESMEditorConstructionScriptProjectSetting::SM_Standard)
	{
		ISMEditorConstructionManager::Get().RunAllConstructionScriptsForBlueprint(GetBlueprintObj());
	}
}

FName FSMStateMachineBlueprintEditor::GetToolkitFName() const
{
	return FName("FSMStateMachineBlueprintEditor");
}

FText FSMStateMachineBlueprintEditor::GetBaseToolkitName() const
{
	return LOCTEXT("SMBlueprintEditorAppLabel", "Logic Driver");
}

FText FSMStateMachineBlueprintEditor::GetToolkitName() const
{
	const TArray<UObject *>& CurrentEditingObjects = GetEditingObjects();
	check(CurrentEditingObjects.Num() > 0);

	const UObject* EditingObject = CurrentEditingObjects[0];

	FFormatNamedArguments Args;
	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));

	return FText::Format(LOCTEXT("FSMStateMachineBlueprintEditor", "{ObjectName}"), Args);
}

FText FSMStateMachineBlueprintEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObject();

	check(EditingObject);

	return GetToolTipTextForObject(EditingObject);
}

FName FSMStateMachineBlueprintEditor::GetEditingAssetTypeName() const
{
	// This is only used under File -> Recent menu, and can open any kind of blueprint (in the appropriate editor).
	// This naming keeps it consistent with normal blueprints, and looks nicer.
	return TEXT("Blueprint");
}

FLinearColor FSMStateMachineBlueprintEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FSMStateMachineBlueprintEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("SMBlueprintEditor");
}

FString FSMStateMachineBlueprintEditor::GetDocumentationLink() const
{
	return TEXT("https://logicdriver.com/docs");
}

void FSMStateMachineBlueprintEditor::Tick(float DeltaTime)
{
	FBlueprintEditor::Tick(DeltaTime);
	
	if (GetCurrentMode() == FSMBlueprintEditorModes::SMPreviewMode && PreviewViewportClient.IsValid())
	{
		PreviewViewportClient.Pin()->OnEditorTick(DeltaTime);
	}
}

void FSMStateMachineBlueprintEditor::ExtendMenu()
{
	if (MenuExtender.IsValid())
	{
		RemoveMenuExtender(MenuExtender);
		MenuExtender.Reset();
	}

	MenuExtender = MakeShareable(new FExtender);
	AddMenuExtender(MenuExtender);

	// Add our extensible menus.
	ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
	AddMenuExtender(SMBlueprintEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	if (bPreviewModeAllowed)
	{
		ISMPreviewEditorModule& SMPreviewEditorModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
		AddMenuExtender(SMPreviewEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	}
	
	if (FSMBlueprintEditorUtils::GetEditorSettings()->bEnableBlueprintMenuExtenders)
	{
		// Add third party menus.
		
		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
		AddMenuExtender(BlueprintEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	}
}

void FSMStateMachineBlueprintEditor::ExtendToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if (ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
	AddToolbarExtender(SMBlueprintEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	if (FSMBlueprintEditorUtils::GetEditorSettings()->bEnableBlueprintToolbarExtenders)
	{
		FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
		AddToolbarExtender(BlueprintEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	}
}

void FSMStateMachineBlueprintEditor::BindCommands()
{
	const FSMEditorCommands& EditorCommands = FSMEditorCommands::Get();
	const TSharedRef<FUICommandList>& UICommandList = GetToolkitCommands();

	// Preview Commands
	if (bPreviewModeAllowed)
	{
		UICommandList->MapAction(
			EditorCommands.StartSimulateStateMachine,
			FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::StartPreviewSimulation),
			FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanStartPreviewSimulation)
		);
		
		UICommandList->MapAction(
			EditorCommands.StopSimulateStateMachine,
			FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::StopPreviewSimulation),
			FCanExecuteAction()
		);
		
		UICommandList->MapAction(
			FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::DeletePreviewSelection),
			FCanExecuteAction()
		);
	}
}

void FSMStateMachineBlueprintEditor::OnDebugObjectSet(UObject* Object)
{
	if (!LoadedBlueprint.IsValid())
	{
		return;
	}

	ResetBlueprintDebugStates();
}

void FSMStateMachineBlueprintEditor::ResetBlueprintDebugStates()
{
	// Locate all sm nodes and reset the debug state to clear up visual ghosting from previous runs.

	TArray<USMGraphNode_Base*> GraphNodes;
	FSMBlueprintEditorUtils::GetAllNodesOfClassNested<USMGraphNode_Base>(LoadedBlueprint.Get(), GraphNodes);

	for (USMGraphNode_Base* GraphNode : GraphNodes)
	{
		GraphNode->ResetDebugState();
	}
}

void FSMStateMachineBlueprintEditor::OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated)
{
	if (!NewlyActivated.IsValid())
	{
		const TArray<UObject*> ObjArray;
		Inspector->ShowDetailsForObjects(ObjArray);
	}
	else
	{
		FBlueprintEditor::OnActiveTabChanged(PreviouslyActive, NewlyActivated);
	}
}

void FSMStateMachineBlueprintEditor::OnSelectedNodesChangedImpl(const TSet<UObject*>& NewSelection)
{
	FBlueprintEditor::OnSelectedNodesChangedImpl(NewSelection);

	if (SelectedStateMachineNode.IsValid())
	{
		SelectedStateMachineNode.Reset();
	}

	// if we only have one node selected, let it know
	if (NewSelection.Num() == 1)
	{
		USMGraphK2Node_Base* NewSelectedStateMachineGraphNode = Cast<USMGraphK2Node_Base>(*NewSelection.CreateConstIterator());
		if (NewSelectedStateMachineGraphNode != nullptr)
		{
			SelectedStateMachineNode = NewSelectedStateMachineGraphNode;
		}
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnSelectedNodesChangedEvent.Broadcast(SharedThis(this), NewSelection);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FSMStateMachineBlueprintEditor::OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList)
{
	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().GoToGraph,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::GoToGraph),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanGoToGraph));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().GoToNodeBlueprint,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::GoToNodeBlueprint),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanGoToNodeBlueprint));
	
	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().CreateSelfTransition,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CreateSingleNodeTransition),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanCreateSingleNodeTransition));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().CutAndMergeStates,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CutCombineStates),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanCutOrCopyCombineStates));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().CopyAndMergeStates,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CopyCombineStates),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanCutOrCopyCombineStates));
	
	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().CollapseToStateMachine,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CollapseNodesToStateMachine),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanCollapseNodesToStateMachine));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ConvertToStateMachineReference,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ConvertStateMachineToReference),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanConvertStateMachineToReference));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ChangeStateMachineReference,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ChangeStateMachineReference),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanChangeStateMachineReference));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().JumpToStateMachineReference,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::JumpToStateMachineReference),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanJumpToStateMachineReference));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().EnableIntermediateGraph,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::EnableIntermediateGraph),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanEnableIntermediateGraph));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().DisableIntermediateGraph,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::DisableIntermediateGraph),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanDisableIntermediateGraph));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ReplaceWithStateMachine,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ReplaceWithStateMachine),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanReplaceWithStateMachine));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ReplaceWithStateMachineReference,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ReplaceWithStateMachineReference),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanReplaceWithStateMachineReference));
	
	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ReplaceWithStateMachineParent,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ReplaceWithStateMachineParent),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanReplaceWithStateMachineParent));
	
	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ReplaceWithState,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ReplaceWithState),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanReplaceWithState));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ReplaceWithConduit,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ReplaceWithConduit),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanReplaceWithConduit));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().GoToPropertyBlueprint,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::GoToPropertyBlueprint),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanGoToPropertyBlueprint));
	
	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().GoToPropertyGraph,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::GoToPropertyGraph),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanGoToPropertyGraph));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().GoToTransitionStackBlueprint,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::GoToTransitionStackBlueprint),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanGoToTransitionStackBlueprint));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ConvertPropertyToGraphEdit,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ToggleGraphPropertyEdit),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanToggleGraphPropertyEdit));

	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().RevertPropertyToNodeEdit,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ToggleGraphPropertyEdit),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanToggleGraphPropertyEdit));
	
	GraphEditorCommandsList->MapAction(FSMEditorCommands::Get().ResetGraphProperty,
		FExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::ClearGraphProperty),
		FCanExecuteAction::CreateSP(this, &FSMStateMachineBlueprintEditor::CanClearGraphProperty));

	OnCreateGraphEditorCommandsEvent.Broadcast(this, GraphEditorCommandsList);
}

void FSMStateMachineBlueprintEditor::CopySelectedNodes()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	TSet<UEdGraphNode*> SelectedNodesSet;

	bool bSelectionChanged = false;
	for (UObject* Object : SelectedNodes)
	{
		if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
		{
			SelectedNodesSet.Add(GraphNode);
		}

		if (const USMGraphNode_RerouteNode* ThisRerouteNode = Cast<USMGraphNode_RerouteNode>(Object))
		{
			// Reroutes prevent individual transitions from being selected by default. Temporarily
			// select all transitions and reroutes to ensure the entire transition reroute chain is pasted.

			TArray<USMGraphNode_TransitionEdge*> ReroutedTransitions;
			TArray<USMGraphNode_RerouteNode*> RerouteNodes;
			ThisRerouteNode->GetAllReroutedTransitions(ReroutedTransitions, RerouteNodes);

			for (USMGraphNode_TransitionEdge* Transition : ReroutedTransitions)
			{
				if (!SelectedNodes.Contains(Transition))
				{
					AddToSelection(Transition);
					bSelectionChanged = true;
				}
			}

			for (USMGraphNode_RerouteNode* RerouteNode : RerouteNodes)
			{
				if (!SelectedNodes.Contains(RerouteNode))
				{
					AddToSelection(RerouteNode);
					bSelectionChanged = true;
				}
			}
		}
	}

	FBlueprintEditor::CopySelectedNodes();

	if (bSelectionChanged)
	{
		// Nodes were added only to make sure they can be pasted, reset the selection back to the original.
		SelectNodes(SelectedNodesSet);
	}
}

void FSMStateMachineBlueprintEditor::PasteNodes()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (UObject* Object : SelectedNodes)
	{
		if (USMGraphNode_Base* Node = Cast<USMGraphNode_Base>(Object))
		{
			SelectedGraphNodesOnPaste.Add(Node);
		}
	}
	
	FBlueprintEditor::PasteNodes();

	SelectedGraphNodesOnPaste.Reset();
}

void FSMStateMachineBlueprintEditor::StartPreviewSimulation()
{
	if (!bPreviewModeAllowed)
	{
		return;
	}
	ISMPreviewEditorModule& PreviewModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
	PreviewModule.StartPreviewSimulation(GetStateMachineBlueprint());
	RegenerateMenusAndToolbars();
}

bool FSMStateMachineBlueprintEditor::CanStartPreviewSimulation() const
{
	ISMPreviewEditorModule& PreviewModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
	return PreviewModule.CanStartPreviewSimulation(GetStateMachineBlueprint());
}

void FSMStateMachineBlueprintEditor::StopPreviewSimulation()
{
	if (!bPreviewModeAllowed)
	{
		return;
	}

	if (USMBlueprint* Blueprint = GetStateMachineBlueprint())
	{
		ISMPreviewEditorModule& PreviewModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
		PreviewModule.StopPreviewSimulation(Blueprint);
	}
}

void FSMStateMachineBlueprintEditor::DeletePreviewSelection()
{
	ISMPreviewEditorModule& PreviewModule = FModuleManager::LoadModuleChecked<ISMPreviewEditorModule>(LOGICDRIVER_PREVIEW_MODULE_NAME);
	PreviewModule.DeleteSelection(SharedThis(this));
}

void FSMStateMachineBlueprintEditor::SetPreviewClient(const TSharedPtr<ISMPreviewModeViewportClient>& InPreviewClient)
{
	PreviewViewportClient = InPreviewClient;
}

void FSMStateMachineBlueprintEditor::CreateSingleNodeTransition()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(Node))
		{
			// This is a hack since we only want the context menu to be the way to self transition.
			StateNode->bCanTransitionToSelf = true;
			StateNode->GetSchema()->TryCreateConnection(StateNode->GetOutputPin(), StateNode->GetInputPin());
			StateNode->bCanTransitionToSelf = false;
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanCreateSingleNodeTransition() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(Node))
		{
			// Skip if already has self transition or it's an Any State Node.
			if (StateNode->HasTransitionFromNode(StateNode) || StateNode->IsA<USMGraphNode_AnyStateNode>() ||
				StateNode->IsA<USMGraphNode_LinkStateNode>())
			{
				continue;
			}

			return true;
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::CollapseNodesToStateMachine()
{
	const TSet<UObject*> Nodes = GetSelectedNodes();

	FSMBlueprintEditorUtils::CollapseNodesAndCreateStateMachine(Nodes);
}

bool FSMStateMachineBlueprintEditor::CanCollapseNodesToStateMachine() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		if (const USMGraphNode_StateNodeBase* StateNode = Cast<USMGraphNode_StateNodeBase>(Node))
		{
			if (StateNode->IsA<USMGraphNode_RerouteNode>())
			{
				continue;
			}

			FSMStateMachineNodePlacementValidator Validator;
			return FSMBlueprintEditorUtils::CanStateMachineBePlacedInGraph(StateNode->GetOwningStateMachineGraph(), Validator);
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::CutCombineStates()
{
	FSMBlueprintEditorUtils::CombineStates(SelectedNodeForContext.Get(), GetSelectedNodes(), true);
	// Clear selection down the right clicked node. When the nodes are removed slate may not update the selection set otherwise and can cause a crash.
	ClearSelectionStateFor(SelectionState_Graph);
	AddToSelection(SelectedNodeForContext.Get());
}

void FSMStateMachineBlueprintEditor::CopyCombineStates()
{
	FSMBlueprintEditorUtils::CombineStates(SelectedNodeForContext.Get(), GetSelectedNodes(), false);
}

bool FSMStateMachineBlueprintEditor::CanCutOrCopyCombineStates() const
{
	if (!Cast<USMGraphNode_StateNode>(SelectedNodeForContext.Get()))
	{
		return false;
	}

	int32 MergeCount = 0;
	TSet<UObject*> Nodes = GetSelectedNodes();
	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateNode* StateNode = Cast<USMGraphNode_StateNode>(Node))
		{
			if (StateNode != SelectedNodeForContext.Get() && StateNode->IsUsingDefaultNodeClass() && StateNode->GetAllNodeStackTemplates().Num() == 0)
			{
				continue;
			}
			
			if (++MergeCount > 1)
			{
				break;
			}
		}
	}

	return MergeCount > 1;
}

void FSMStateMachineBlueprintEditor::ConvertStateMachineToReference()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	USMGraphNode_StateMachineStateNode* StateMachineNode = nullptr;

	for (UObject* Node : Nodes)
	{
		StateMachineNode = CastChecked<USMGraphNode_StateMachineStateNode>(Node);
		break;
	}

	FSMBlueprintEditorUtils::ConvertStateMachineToReference(StateMachineNode);
}

bool FSMStateMachineBlueprintEditor::CanConvertStateMachineToReference() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		if (Node->IsA<USMGraphNode_StateMachineParentNode>())
		{
			continue;
		}
		
		if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
		{
			if (!StateNode->IsStateMachineReference())
			{
				return FSMBlueprintEditorUtils::CanStateMachineBeConvertedToReference(StateNode->GetOwningStateMachineGraph());
			}
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::ChangeStateMachineReference()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FOpenAssetDialogConfig SelectAssetConfig;
	SelectAssetConfig.DialogTitleOverride = LOCTEXT("ChooseStateMachinePath", "Choose a state machine");
	SelectAssetConfig.bAllowMultipleSelection = false;
	SelectAssetConfig.AssetClassNames.Add(USMBlueprint::StaticClass()->GetClassPathName());

	// Set the path to the current folder.
	if (UBlueprint* Blueprint = GetBlueprintObj())
	{
		UObject* AssetOuter = Blueprint->GetOuter();
		UPackage* AssetPackage = AssetOuter->GetOutermost();

		// Remove the file name and go directly to the folder.
		FString AssetPath = AssetPackage->GetName();
		const int LastSlashPos = AssetPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		SelectAssetConfig.DefaultPath = AssetPath.Left(LastSlashPos);
	}

	TArray<FAssetData> AssetData = ContentBrowserModule.Get().CreateModalOpenAssetDialog(SelectAssetConfig);
	if (AssetData.Num() == 1)
	{
		if (USMBlueprint *ReferencedBlueprint = Cast<USMBlueprint>(AssetData[0].GetAsset()))
		{
			if (!ReferencedBlueprint->HasAnyFlags(RF_Transient) && IsValid(ReferencedBlueprint))
			{
				TSet<UObject*> Nodes = GetSelectedNodes();
				for (UObject* Node : Nodes)
				{
					if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
					{
						FScopedTransaction Transaction(TEXT(""), NSLOCTEXT("UnrealEd", "ChangeStateMachineReference", "Change State Machine Reference"), StateNode);
						StateNode->Modify();
						if (!StateNode->ReferenceStateMachine(ReferencedBlueprint))
						{
							Transaction.Cancel();
							return;
						}

						UBlueprint* Blueprint = FSMBlueprintEditorUtils::FindBlueprintForNodeChecked(StateNode);
						FSMBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
					}
				}
			}
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanChangeStateMachineReference() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
		{
			return StateNode->IsStateMachineReference();
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::JumpToStateMachineReference()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
		{
			StateNode->JumpToReference();
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanJumpToStateMachineReference() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
		{
			return StateNode->IsStateMachineReference();
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::EnableIntermediateGraph()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
		{
			StateNode->SetUseIntermediateGraph(true);
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanEnableIntermediateGraph() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
		{
			return !StateNode->ShouldUseIntermediateGraph();
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::DisableIntermediateGraph()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
		{
			StateNode->SetUseIntermediateGraph(false);
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanDisableIntermediateGraph() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineStateNode* StateNode = Cast<USMGraphNode_StateMachineStateNode>(Node))
		{
			return StateNode->ShouldUseIntermediateGraph();
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::ReplaceWithStateMachine()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(Cast<USMGraphNode_Base>(Node));
	}
}

bool FSMStateMachineBlueprintEditor::CanReplaceWithStateMachine() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		bool bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent;
		USMGraphSchema::CanReplaceNodeWith(Cast<UEdGraphNode>(Node), bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent);

		return bCanAddStateMachine;
	}

	return false;
}

void FSMStateMachineBlueprintEditor::ReplaceWithStateMachineReference()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		if (USMGraphNode_StateMachineStateNode* StateMachineRefNode = FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineStateNode>(Cast<USMGraphNode_Base>(Node), true))
		{
			StateMachineRefNode->ReferenceStateMachine(nullptr);
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanReplaceWithStateMachineReference() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		bool bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent;
		USMGraphSchema::CanReplaceNodeWith(Cast<UEdGraphNode>(Node), bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent);

		return bCanAddStateMachineRef;
	}

	return false;
}

void FSMStateMachineBlueprintEditor::ReplaceWithStateMachineParent()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateMachineParentNode>(Cast<USMGraphNode_Base>(Node));
	}
}

bool FSMStateMachineBlueprintEditor::CanReplaceWithStateMachineParent() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		bool bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent;
		USMGraphSchema::CanReplaceNodeWith(Cast<UEdGraphNode>(Node), bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent);

		return bCanAddStateMachineParent;
	}

	return false;
}

void FSMStateMachineBlueprintEditor::ReplaceWithState()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_StateNode>(Cast<USMGraphNode_Base>(Node));
	}
}

bool FSMStateMachineBlueprintEditor::CanReplaceWithState() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		bool bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent;
		USMGraphSchema::CanReplaceNodeWith(Cast<UEdGraphNode>(Node), bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent);

		return bCanAddState;
	}

	return false;
}

void FSMStateMachineBlueprintEditor::ReplaceWithConduit()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	for (UObject* Node : Nodes)
	{
		FSMBlueprintEditorUtils::ConvertNodeTo<USMGraphNode_ConduitNode>(Cast<USMGraphNode_Base>(Node));
	}
}

bool FSMStateMachineBlueprintEditor::CanReplaceWithConduit() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		bool bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent;
		USMGraphSchema::CanReplaceNodeWith(Cast<UEdGraphNode>(Node), bCanAddStateMachine, bCanAddStateMachineRef, bCanAddState, bCanAddConduit, bCanAddStateMachineParent);

		return bCanAddConduit;
	}

	return false;
}

void FSMStateMachineBlueprintEditor::GoToGraph()
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return;
	}

	for (UObject* Node : Nodes)
	{
		if (const USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(Node))
		{
			GraphNode->GoToLocalGraph();
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanGoToGraph() const
{
	TSet<UObject*> Nodes = GetSelectedNodes();

	if (Nodes.Num() != 1)
	{
		return false;
	}

	for (UObject* Node : Nodes)
	{
		if (const USMGraphNode_Base* GraphNode = Cast<USMGraphNode_Base>(Node))
		{
			if (!GraphNode->CanGoToLocalGraph())
			{
				continue;
			}

			return true;
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::GoToNodeBlueprint()
{
	if (USMGraphNode_Base* Node = Cast<USMGraphNode_Base>(GetSingleSelectedNode()))
	{
		FSMBlueprintEditorUtils::GoToNodeBlueprint(Node);
	}
}

bool FSMStateMachineBlueprintEditor::CanGoToNodeBlueprint() const
{
	if (USMGraphNode_Base* Node = Cast<USMGraphNode_Base>(GetSingleSelectedNode()))
	{
		if (UClass* Class = Node->GetNodeClass())
		{
			if (UBlueprint::GetBlueprintFromClass(Class))
			{
				return true;
			}
		}
	}

	return false;
}

void FSMStateMachineBlueprintEditor::GoToPropertyBlueprint()
{
	if (USMGraphNode_StateNode* Node = Cast<USMGraphNode_StateNode>(SelectedPropertyNode->GetOwningGraphNode()))
	{
		if (UBlueprint* PropertyBlueprint = SelectedPropertyNode->GetTemplateBlueprint())
		{
			if (USMNodeInstance* Template = SelectedPropertyNode->GetOwningTemplate())
			{
				if (const FSMNode_Base* DebugNode = FSMBlueprintEditorUtils::GetDebugNode(Node))
				{
					const int32 TemplateIndex = Node->GetIndexOfTemplate(Template->GetTemplateGuid());
					if (USMNodeInstance* NodeStackInstance = DebugNode->GetNodeInStack(TemplateIndex))
					{
						PropertyBlueprint->SetObjectBeingDebugged(NodeStackInstance);
					}
					else if (USMNodeInstance* NodeInstance = DebugNode->GetNodeInstance())
					{
						if (UBlueprint* NodeBlueprint = UBlueprint::GetBlueprintFromClass(NodeInstance->GetClass()))
						{
							// Node template not found, verify this is the node blueprint instead.
							if (NodeBlueprint == PropertyBlueprint)
							{
								PropertyBlueprint->SetObjectBeingDebugged(NodeInstance);
							}
						}
					}
				}
			}
			
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(PropertyBlueprint);
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanGoToPropertyBlueprint() const
{
	return IsSelectedPropertyNodeValid() && SelectedPropertyNode->GetTemplateBlueprint() != nullptr;
}

void FSMStateMachineBlueprintEditor::GoToTransitionStackBlueprint()
{
	if (const USMGraphNode_TransitionEdge* Node = Cast<USMGraphNode_TransitionEdge>(GetSingleSelectedNode()))
	{
		if (const USMNodeInstance* Template = Node->GetHoveredStackTemplate())
		{
			if (UBlueprint* NodeBlueprint = UBlueprint::GetBlueprintFromClass(Template->GetClass()))
			{
				if (const FSMNode_Base* DebugNode = FSMBlueprintEditorUtils::GetDebugNode(Node))
				{
					const TArray<FTransitionStackContainer>& AllStackInstances = Node->GetAllNodeStackTemplates();

					// Find exact index.
					int32 StackIndex = INDEX_NONE;
					for (int32 Idx = 0; Idx < AllStackInstances.Num(); ++Idx)
					{
						if (AllStackInstances[Idx].NodeStackInstanceTemplate == Template)
						{
							StackIndex = Idx;
							break;
						}
					}

					if (ensure(StackIndex >= 0))
					{
						if (USMNodeInstance* NodeInstance = DebugNode->GetNodeInStack(StackIndex))
						{
							NodeBlueprint->SetObjectBeingDebugged(NodeInstance);
						}
					}
				}

				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NodeBlueprint);
			}
		}
	}
}

bool FSMStateMachineBlueprintEditor::CanGoToTransitionStackBlueprint() const
{
	// Command is added in only if hovered.
	return true;
}

void FSMStateMachineBlueprintEditor::GoToPropertyGraph()
{
	SelectedPropertyNode->JumpToPropertyGraph();
}

bool FSMStateMachineBlueprintEditor::CanGoToPropertyGraph() const
{
	return IsSelectedPropertyNodeValid(false);
}

void FSMStateMachineBlueprintEditor::ClearGraphProperty()
{
	SelectedPropertyNode->ResetProperty();
}

bool FSMStateMachineBlueprintEditor::CanClearGraphProperty() const
{
	return IsSelectedPropertyNodeValid();
}

void FSMStateMachineBlueprintEditor::ToggleGraphPropertyEdit()
{
	SelectedPropertyNode->GetPropertyGraph()->ToggleGraphPropertyEdit();
}

bool FSMStateMachineBlueprintEditor::CanToggleGraphPropertyEdit() const
{
	if (IsSelectedPropertyNodeValid())
	{
		return SelectedPropertyNode->GetPropertyNodeChecked()->AllowToggleGraphEdit();
	}

	return false;
}

#if !LOGICDRIVER_HAS_PROPER_VARIABLE_CUSTOMIZATION
TSet<FSMNodeBlueprintEditor*> FSMNodeBlueprintEditor::AllNodeBlueprintEditors;
#endif

FSMNodeBlueprintEditor::FSMNodeBlueprintEditor()
{
}

FSMNodeBlueprintEditor::~FSMNodeBlueprintEditor()
{
#if !LOGICDRIVER_HAS_PROPER_VARIABLE_CUSTOMIZATION
	AllNodeBlueprintEditors.Remove(this);

	if (AllNodeBlueprintEditors.Num() == 0 && bVariablesCustomized)
	{
		// When there are no more node editors open, remove our variable customization.
		ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
		SMBlueprintEditorModule.UnregisterBlueprintVariableCustomization();
		bVariablesCustomized = false;
	}
#endif
}

FText FSMNodeBlueprintEditor::GetBaseToolkitName() const
{
	return LOCTEXT("SMNodeBlueprintEditorAppLabel", "Logic Driver");
}

FName FSMNodeBlueprintEditor::GetEditingAssetTypeName() const
{
	// This is only used under File -> Recent menu, and can open any kind of blueprint (in the appropriate editor).
	// This naming keeps it consistent with normal blueprints, and looks nicer.
	return TEXT("Blueprint");
}

FString FSMNodeBlueprintEditor::GetDocumentationLink() const
{
	return TEXT("https://logicdriver.com/docs");
}

FGraphAppearanceInfo FSMNodeBlueprintEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = FBlueprintEditor::GetGraphAppearance(InGraph);

	if (Cast<USMNodeBlueprint>(GetBlueprintObj()))
	{
		LD::EditorAuth::LDAuthStatus AuthenticationStatus = LD::EditorAuth::GetUserMarketplaceAuthenticationStatus();

		if (AuthenticationStatus == LD::EditorAuth::LDAuthStatus::Succeeded || FSMAuthenticator::Get().IsAuthenticated())
		{
			AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_NodeBlueprint", "NODE BLUEPRINT");
		}
		else
		{
			AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_NodeBlueprintInvalid", "LOGIC DRIVER (INVALID LICENSE)");
		}
	}

	return MoveTemp(AppearanceInfo);
}

void FSMNodeBlueprintEditor::InitNodeBlueprintEditor(const EToolkitMode::Type Mode,
                                                     const TSharedPtr<IToolkitHost>& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints,
                                                     bool bShouldOpenInDefaultsMode)
{
	InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);

#if !LOGICDRIVER_HAS_PROPER_VARIABLE_CUSTOMIZATION
	AllNodeBlueprintEditors.Add(this);
	if (FSMBlueprintEditorUtils::GetProjectEditorSettings()->bEnableVariableCustomization)
	{
		ISMSystemEditorModule& SMBlueprintEditorModule = FModuleManager::GetModuleChecked<ISMSystemEditorModule>(LOGICDRIVER_EDITOR_MODULE_NAME);
		SMBlueprintEditorModule.RegisterBlueprintVariableCustomization();
		bVariablesCustomized = true;
	}
#endif
}

void FSMNodeBlueprintEditor::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled)
{
	FBlueprintEditor::OnBlueprintChangedImpl(InBlueprint, bIsJustBeingCompiled);

	TArray<UBlueprint*> Blueprints;
	FSMBlueprintEditorUtils::GetDependentBlueprints(InBlueprint, Blueprints);

	for (UBlueprint* Blueprint : Blueprints)
	{
		if (Blueprint->IsA<USMBlueprint>() && !Blueprint->bIsRegeneratingOnLoad)
		{
			// First check for a circular dependency where this blueprint is also dependent on its dependent.
			// In that case we just want to continue because UE will handle this. Otherwise the compile fails with little information.
			TArray<UBlueprint*> OtherBlueprints;
			FSMBlueprintEditorUtils::GetDependentBlueprints(Blueprint, OtherBlueprints);
			if (OtherBlueprints.Contains(InBlueprint))
			{
				continue;
			}

			// TODO: The first call to ensure cached dependencies may fix a rare crash involving a REINST template class of this class.
			FSMBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(Blueprint);
			if (bIsJustBeingCompiled)
			{
				/* If this is part of a compile (compile button pressed) then attempt a full compile of the state machine. This will
				 * also refresh graph properties exposed on the node. */
				FSMBlueprintEditorUtils::ConditionallyCompileBlueprint(Blueprint, true, true);
			}
			else
			{
				// A change not caused by a compile. Mark the state machine dirty so it can be recompiled later.
				// Don't just change compile state, mark the asset dirty so it can save structural or property
				// changes. Especially important if a parent node calls this.
				FSMBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
