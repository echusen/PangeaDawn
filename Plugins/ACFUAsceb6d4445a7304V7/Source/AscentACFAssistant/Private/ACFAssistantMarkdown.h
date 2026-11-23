// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SGridPanel;
class SMultiLineEditableText;
namespace ACFAssistantMarkdown
{
	FString ToRichText(const FString& Markdown);
	class SSelectableBody : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSelectableBody) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		void SetMarkdown(const FString& Markdown);

	private:
		// One prose run or one table, in the order they appear. A block keeps its widgets across updates so streaming only changes text.
		struct FBlock
		{
			TSharedPtr<SWidget> Root;
			TSharedPtr<SMultiLineEditableText> Prose;
			FString ProseText;
			TSharedPtr<SGridPanel> Grid;
			TArray<EHorizontalAlignment> Alignments;
			TArray<TArray<TSharedPtr<SMultiLineEditableText>>> Cells;
			TArray<TArray<FString>> CellText;
		};

		void SetProse(int32 Index, const FString& Markdown);
		void SetTable(int32 Index, const TArray<TArray<FString>>& Rows, const TArray<EHorizontalAlignment>& Alignments);
		void RemoveBlocksFrom(int32 Index);

		TSharedPtr<SVerticalBox> Content;
		TArray<FBlock> Blocks;
	};

	TSharedRef<SSelectableBody> CreateSelectableBody(const FString& Markdown);
	void SetBodyText(const TSharedRef<SSelectableBody>& Widget, const FString& Markdown);
}
