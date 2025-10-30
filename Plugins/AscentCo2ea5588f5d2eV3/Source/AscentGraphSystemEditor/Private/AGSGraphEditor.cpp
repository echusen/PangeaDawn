// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 

#include "AssetTypeActions_AGSGraph.h"
#include "AGSGraphEditorPCH.h"
#include "EdGraphUtilities.h"
#include "AssetEditor/EdNode_AGSGraphNode.h"
#include "AssetEditor/EdNode_AGSGraphEdge.h"
#include "AssetEditor/SEdNode_AGSGraphNode.h"
#include "AssetEditor/SEdNode_AGSGraphEdge.h"


DEFINE_LOG_CATEGORY(AGSGraphEditor)

#define LOCTEXT_NAMESPACE "Editor_AGSGraph"

class FGraphPanelNodeFactory_AGSGraph : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UEdNode_AGSGraphNode* EdNode_GraphNode = Cast<UEdNode_AGSGraphNode>(Node))
		{
			return SNew(SEdNode_AGSGraphNode, EdNode_GraphNode);
		}
		else if (UEdNode_AGSGraphEdge* EdNode_Edge = Cast<UEdNode_AGSGraphEdge>(Node))
		{
			return SNew(SEdNode_AGSGraphEdge, EdNode_Edge);
		}
		return nullptr;
	}
};

TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory_AGSGraph;

class FAGSGraphEditor : public IAGSGraphEditor
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

private:
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	EAssetTypeCategories::Type AGSGraphAssetCategoryBit;
};

IMPLEMENT_MODULE( FAGSGraphEditor, AGSGraphEditor )

void FAGSGraphEditor::StartupModule()
{

	GraphPanelNodeFactory_AGSGraph = MakeShareable(new FGraphPanelNodeFactory_AGSGraph());
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory_AGSGraph);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AGSGraphAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("ACF Tools")), LOCTEXT("AGSGraphAssetCategory", "ACF Tools"));

	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_AGSGraph(AGSGraphAssetCategoryBit)));
}


void FAGSGraphEditor::ShutdownModule()
{
	// Unregister all the asset types that we registered
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (int32 Index = 0; Index < CreatedAssetTypeActions.Num(); ++Index)
		{
			AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[Index].ToSharedRef());
		}
	}

	if (GraphPanelNodeFactory_AGSGraph.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory_AGSGraph);
		GraphPanelNodeFactory_AGSGraph.Reset();
	}

}

void FAGSGraphEditor::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

#undef LOCTEXT_NAMESPACE

