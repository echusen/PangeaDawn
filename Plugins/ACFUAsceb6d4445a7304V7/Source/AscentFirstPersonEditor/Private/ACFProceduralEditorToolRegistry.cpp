// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFProceduralEditorToolRegistry.h"

#include "ACFFirstPersonEditorSettings.h"
#include "ACFFirstPersonLog.h"

FACFProceduralEditorToolRegistry& FACFProceduralEditorToolRegistry::Get()
{
	static FACFProceduralEditorToolRegistry Instance;
	return Instance;
}

void FACFProceduralEditorToolRegistry::RegisterTool(const FACFProceduralEditorToolDescriptor& Descriptor)
{
	if (Descriptor.Id.IsNone())
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("Editor tool registered without an id. Ignored."));
		return;
	}

	if (!Descriptor.SpawnTab.IsBound())
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("Editor tool '%s' has no tab spawner. Ignored."), *Descriptor.Id.ToString());
		return;
	}

	if (FindTool(Descriptor.Id))
	{
		UE_LOG(LogACFFirstPerson, Warning, TEXT("Editor tool '%s' is already registered. Ignored."), *Descriptor.Id.ToString());
		return;
	}

	Tools.Add(Descriptor);

	FTabSpawnerEntry& Spawner = FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(Descriptor.Id, Descriptor.SpawnTab)
		.SetDisplayName(Descriptor.Label)
		.SetTooltipText(Descriptor.Tooltip)
		.SetIcon(Descriptor.Icon);

	if (WorkspaceGroup.IsValid())
	{
		Spawner.SetGroup(WorkspaceGroup.ToSharedRef());
	}
}

void FACFProceduralEditorToolRegistry::UnregisterTool(const FName& Id)
{
	if (Tools.RemoveAll([&Id](const FACFProceduralEditorToolDescriptor& Tool) { return Tool.Id == Id; }) > 0)
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Id);
	}
}

void FACFProceduralEditorToolRegistry::UnregisterAll()
{
	for (const FACFProceduralEditorToolDescriptor& Tool : Tools)
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Tool.Id);
	}

	Tools.Reset();
	WorkspaceGroup.Reset();
}

const FACFProceduralEditorToolDescriptor* FACFProceduralEditorToolRegistry::FindTool(const FName& Id) const
{
	return Tools.FindByPredicate([&Id](const FACFProceduralEditorToolDescriptor& Tool) { return Tool.Id == Id; });
}

TArray<FACFProceduralResolvedTool> FACFProceduralEditorToolRegistry::GetVisibleTools() const
{
	const UACFFirstPersonEditorSettings* Settings = UACFFirstPersonEditorSettings::Get();

	TArray<FACFProceduralResolvedTool> Visible;
	Visible.Reserve(Tools.Num());

	for (const FACFProceduralEditorToolDescriptor& Tool : Tools)
	{
		const FACFProceduralToolOverride* Override = Settings ? Settings->FindToolOverride(Tool.Id) : nullptr;
		if (Override && Override->bHidden)
		{
			continue;
		}

		// Anything left blank on the override keeps what the tool registered in C++.
		FACFProceduralResolvedTool& Resolved = Visible.AddDefaulted_GetRef();
		Resolved.Id = Tool.Id;
		Resolved.Label = Override && !Override->Label.IsEmpty() ? Override->Label : Tool.Label;
		Resolved.Tooltip = Override && !Override->Tooltip.IsEmpty() ? Override->Tooltip : Tool.Tooltip;
		Resolved.Section = Override && !Override->Section.IsNone() ? Override->Section : Tool.Section;
		Resolved.SectionLabel = Tool.SectionLabel;
		Resolved.Order = Override && Override->Order >= 0 ? Override->Order : Tool.Order;
		Resolved.Icon = Override ? Override->Icon.Resolve(Tool.Icon) : Tool.Icon;

		// A renamed section has no built-in heading to inherit.
		if (Override && !Override->Section.IsNone() && Override->Section != Tool.Section)
		{
			Resolved.SectionLabel = FText::FromName(Override->Section);
		}
	}

	// Sections land in first-encounter order, so Order alone decides both section and row placement.
	Visible.StableSort([](const FACFProceduralResolvedTool& A, const FACFProceduralResolvedTool& B)
	{
		return A.Order < B.Order;
	});

	return Visible;
}
