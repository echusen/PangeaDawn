// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

/**
 * Expansion state remembered by row key rather than by row object.
 *
 * Both trees rebuild their rows from live data, so a row object is destroyed and recreated whenever its
 * source briefly disappears — a stage with no components mid-swap, a weapon being stowed. STreeView keys
 * expansion by item pointer, so the recreated row comes back collapsed. Keying by a stable string instead
 * survives that, and survives changing the selected entity.
 */
namespace ACFProceduralTreeExpansion
{
	/** Applies the remembered state for Row's key, falling back to bDefaultExpanded for a key never seen. */
	template<typename TreeType, typename RowType>
	void ApplyRemembered(TreeType& Tree, TMap<FString, bool>& StateByKey, const TSharedPtr<RowType>& Row, bool bDefaultExpanded)
	{
		if (!Row.IsValid())
		{
			return;
		}

		const bool bExpanded = StateByKey.FindOrAdd(Row->Key, bDefaultExpanded);
		Tree.SetItemExpansion(Row, bExpanded);
	}

	/** Records what the user just did, so a later rebuild restores it. */
	template<typename RowType>
	void Remember(TMap<FString, bool>& StateByKey, const TSharedPtr<RowType>& Row, bool bExpanded)
	{
		if (Row.IsValid())
		{
			StateByKey.Add(Row->Key, bExpanded);
		}
	}

	/** Expands or collapses Row and everything under it. */
	template<typename TreeType, typename RowType>
	void SetSubtree(TreeType& Tree, TMap<FString, bool>& StateByKey, const TSharedPtr<RowType>& Row, bool bExpand)
	{
		if (!Row.IsValid())
		{
			return;
		}

		StateByKey.Add(Row->Key, bExpand);
		Tree.SetItemExpansion(Row, bExpand);

		for (const TSharedPtr<RowType>& Child : Row->Children)
		{
			SetSubtree(Tree, StateByKey, Child, bExpand);
		}
	}

	template<typename TreeType, typename RowType>
	void SetAll(TreeType& Tree, TMap<FString, bool>& StateByKey, const TArray<TSharedPtr<RowType>>& Roots, bool bExpand)
	{
		for (const TSharedPtr<RowType>& Root : Roots)
		{
			SetSubtree(Tree, StateByKey, Root, bExpand);
		}
	}

	/** Expand / collapse subtree entries for the row under the cursor. */
	template<typename TreeType, typename RowType>
	TSharedPtr<SWidget> BuildContextMenu(TreeType& Tree, TMap<FString, bool>& StateByKey, const TSharedPtr<RowType>& Row)
	{
		if (!Row.IsValid() || Row->Children.Num() == 0)
		{
			return nullptr;
		}

		FMenuBuilder MenuBuilder(true, nullptr);

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("ACFProceduralTreeExpansion", "ExpandSubtree", "Expand All Below"),
			NSLOCTEXT("ACFProceduralTreeExpansion", "ExpandSubtreeTooltip", "Expand this row and everything under it."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([&Tree, &StateByKey, Row]()
			{
				SetSubtree(Tree, StateByKey, Row, true);
			})));

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("ACFProceduralTreeExpansion", "CollapseSubtree", "Collapse All Below"),
			NSLOCTEXT("ACFProceduralTreeExpansion", "CollapseSubtreeTooltip", "Collapse this row and everything under it."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([&Tree, &StateByKey, Row]()
			{
				SetSubtree(Tree, StateByKey, Row, false);
			})));

		return MenuBuilder.MakeWidget();
	}
}
