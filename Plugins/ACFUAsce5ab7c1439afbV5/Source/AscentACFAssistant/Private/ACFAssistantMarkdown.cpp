// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantMarkdown.h"
#include "ACFAssistantSettings.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"

namespace ACFAssistantMarkdown
{
	TSharedPtr<FSlateStyleSet> GMarkdownStyle;
	FSlateFontInfo GAppliedChatFont;

	FSlateFontInfo WithTypefaceAndSize(const FSlateFontInfo& Base, const FName Typeface, const int32 Size)
	{
		FSlateFontInfo Font = Base;
		if (!Typeface.IsNone()) { Font.TypefaceFontName = Typeface; }
		Font.Size = FMath::Max(6, Size);
		return Font;
	}

	void AppendEscapedChar(FString& Output, const TCHAR Character)
	{
		if (Character == TEXT('&')) { Output += TEXT("&amp;"); }
		else if (Character == TEXT('<')) { Output += TEXT("&lt;"); }
		else if (Character == TEXT('>')) { Output += TEXT("&gt;"); }
		else { Output.AppendChar(Character); }
	}

	const FTextBlockStyle& BodyTextStyle()
	{
		static const FTextBlockStyle Style = []
		{
			FTextBlockStyle Result = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
			Result.SetColorAndOpacity(FLinearColor::FromSRGBColor(FColor(242, 242, 245)));
			return Result;
		}();
		return Style;
	}

	FLinearColor KhakiHighlight() { return FLinearColor::FromSRGBColor(FColor(201, 194, 166)); }

	const TCHAR* HeadingStyleName(const int32 Level)
	{
		if (Level <= 1) { return TEXT("Heading1"); }
		if (Level == 2) { return TEXT("Heading2"); }
		return TEXT("Heading3");
	}

	const ISlateStyle* GetMarkdownStyle()
	{
		const FSlateFontInfo ChatFont = UACFAssistantSettings::Get().ResolvedChatFont();
		if (!GMarkdownStyle.IsValid() || !(GAppliedChatFont == ChatFont))
		{
			GAppliedChatFont = ChatFont;
			GMarkdownStyle = MakeShared<FSlateStyleSet>(TEXT("ACFAssistantMarkdown"));
			GMarkdownStyle->SetParentStyleName(FAppStyle::GetAppStyleSetName());
			const FTextBlockStyle Normal = BodyTextStyle();
			const int32 Size = FMath::Max(6, ChatFont.Size);
			FTextBlockStyle Bold = Normal;
			Bold.SetFont(WithTypefaceAndSize(ChatFont, TEXT("Bold"), Size));
			FTextBlockStyle Italic = Normal;
			Italic.SetFont(WithTypefaceAndSize(ChatFont, TEXT("Italic"), Size));
			FTextBlockStyle Code = Normal;
			Code.SetFont(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansMono.ttf"), FMath::Max(6, Size - 1)));
			Code.SetColorAndOpacity(KhakiHighlight());
			FTextBlockStyle Heading1 = Normal;
			Heading1.SetFont(WithTypefaceAndSize(ChatFont, TEXT("Bold"), Size + 4));
			FTextBlockStyle Heading2 = Normal;
			Heading2.SetFont(WithTypefaceAndSize(ChatFont, TEXT("Bold"), Size + 2));
			FTextBlockStyle Heading3 = Normal;
			Heading3.SetFont(WithTypefaceAndSize(ChatFont, TEXT("Bold"), Size + 1));
			Heading1.SetColorAndOpacity(KhakiHighlight());
			Heading2.SetColorAndOpacity(KhakiHighlight());
			Heading3.SetColorAndOpacity(KhakiHighlight());
			GMarkdownStyle->Set(TEXT("Bold"), Bold);
			GMarkdownStyle->Set(TEXT("Italic"), Italic);
			GMarkdownStyle->Set(TEXT("Code"), Code);
			GMarkdownStyle->Set(TEXT("Heading1"), Heading1);
			GMarkdownStyle->Set(TEXT("Heading2"), Heading2);
			GMarkdownStyle->Set(TEXT("Heading3"), Heading3);
		}
		return GMarkdownStyle.Get();
	}

	FString ToRichText(const FString& Markdown)
	{
		FString Result;
		Result.Reserve(Markdown.Len());
		FString Span;
		bool bBold = false;
		bool bItalic = false;
		bool bInlineCode = false;
		bool bCodeBlock = false;
		bool bHeading = false;
		bool bLineStart = true;
		int32 HeadingLevel = 0;

		// A tagged Slate run accepts only one style. Close the current run before switching bold, code, or heading.
		const auto CommitRun = [&]()
		{
			if (Span.IsEmpty()) { return; }
			const TCHAR* StyleName = nullptr;
			if (bInlineCode || bCodeBlock) { StyleName = TEXT("Code"); }
			else if (bHeading) { StyleName = HeadingStyleName(HeadingLevel); }
			else if (bBold) { StyleName = TEXT("Bold"); }
			else if (bItalic) { StyleName = TEXT("Italic"); }
			if (StyleName)
			{
				Result += TEXT("<");
				Result += StyleName;
				Result += TEXT(">");
				Result += Span;
				Result += TEXT("</>");
			}
			else
			{
				Result += Span;
			}
			Span.Reset();
		};

		for (int32 Index = 0; Index < Markdown.Len(); ++Index)
		{
			if (bLineStart && !bInlineCode && Index + 2 < Markdown.Len()
				&& Markdown[Index] == TEXT('`') && Markdown[Index + 1] == TEXT('`') && Markdown[Index + 2] == TEXT('`'))
			{
				CommitRun();
				bCodeBlock = !bCodeBlock;
				bHeading = false;
				HeadingLevel = 0;
				Index += 2;
				while (Index + 1 < Markdown.Len() && Markdown[Index + 1] != TEXT('\n') && Markdown[Index + 1] != TEXT('\r'))
				{
					++Index;
				}
				bLineStart = false;
				continue;
			}

			if (bLineStart && !bInlineCode && !bCodeBlock)
			{
				int32 Cursor = Index;
				int32 LeadingSpaces = 0;
				while (Cursor < Markdown.Len() && Markdown[Cursor] == TEXT(' ') && LeadingSpaces < 3)
				{
					++Cursor;
					++LeadingSpaces;
				}
				int32 HashCount = 0;
				while (Cursor < Markdown.Len() && Markdown[Cursor] == TEXT('#') && HashCount < 6)
				{
					++Cursor;
					++HashCount;
				}
				if (HashCount > 0 && Cursor < Markdown.Len() && (Markdown[Cursor] == TEXT(' ') || Markdown[Cursor] == TEXT('\t')))
				{
					while (Cursor < Markdown.Len() && (Markdown[Cursor] == TEXT(' ') || Markdown[Cursor] == TEXT('\t'))) { ++Cursor; }
					CommitRun();
					bHeading = true;
					HeadingLevel = HashCount;
					Index = Cursor - 1;
					bLineStart = false;
					continue;
				}
				if (Cursor + 1 < Markdown.Len() && Markdown[Cursor + 1] == TEXT(' ')
					&& (Markdown[Cursor] == TEXT('-') || Markdown[Cursor] == TEXT('*') || Markdown[Cursor] == TEXT('+')))
				{
					Span.AppendChars(*Markdown + Index, Cursor - Index);
					Span.AppendChar(0x2022);
					Index = Cursor;
					bLineStart = false;
					continue;
				}
			}

			if (!bCodeBlock && !bInlineCode && Index + 1 < Markdown.Len()
				&& Markdown[Index] == TEXT('*') && Markdown[Index + 1] == TEXT('*'))
			{
				CommitRun();
				bBold = !bBold;
				++Index;
				bLineStart = false;
				continue;
			}

			if (!bCodeBlock && Markdown[Index] == TEXT('`'))
			{
				CommitRun();
				bInlineCode = !bInlineCode;
				bLineStart = false;
				continue;
			}

			if (!bCodeBlock && !bInlineCode && Markdown[Index] == TEXT('*'))
			{
				CommitRun();
				bItalic = !bItalic;
				bLineStart = false;
				continue;
			}

			if (Markdown[Index] == TEXT('\r') || Markdown[Index] == TEXT('\n'))
			{
				CommitRun();
				Result += Markdown[Index];
				if (Markdown[Index] == TEXT('\r') && Index + 1 < Markdown.Len() && Markdown[Index + 1] == TEXT('\n'))
				{
					++Index;
					Result += TEXT('\n');
				}
				bHeading = false;
				HeadingLevel = 0;
				bLineStart = true;
				continue;
			}

			AppendEscapedChar(Span, Markdown[Index]);
			bLineStart = false;
		}

		CommitRun();
		return Result;
	}

	namespace
	{
		struct FMarkdownLine
		{
			FString Text;
			int32 Start;
			int32 End;
		};

		bool SplitTableRow(const FString& Line, TArray<FString>& Cells)
		{
			Cells.Reset();
			FString Cell;
			int32 CodeTicks = 0;
			bool bSeparator = false;
			bool bFirstPipe = false;
			bool bLastPipe = false;
			for (int32 Index = 0; Index < Line.Len(); ++Index)
			{
				const TCHAR Character = Line[Index];
				if (Character == TEXT('\\') && Index + 1 < Line.Len())
				{
					if (Line[Index + 1] == TEXT('|'))
					{
						Cell.AppendChar(TEXT('|'));
						++Index;
						bLastPipe = false;
						continue;
					}
					Cell.AppendChar(Character);
					Cell.AppendChar(Line[++Index]);
					bLastPipe = false;
					continue;
				}
				if (Character == TEXT('`'))
				{
					int32 Count = 1;
					while (Index + Count < Line.Len() && Line[Index + Count] == TEXT('`')) { ++Count; }
					if (CodeTicks == Count) { CodeTicks = 0; }
					else if (CodeTicks == 0)
					{
						const FString Marker = FString::ChrN(Count, TEXT('`'));
						if (Line.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + Count) != INDEX_NONE)
						{
							CodeTicks = Count;
						}
					}
					Cell.AppendChars(*Line + Index, Count);
					Index += Count - 1;
					bLastPipe = false;
					continue;
				}
				if (Character == TEXT('|') && CodeTicks == 0)
				{
					if (!bSeparator && Cell.TrimStartAndEnd().IsEmpty()) { bFirstPipe = true; }
					Cells.Add(Cell.TrimStartAndEnd());
					Cell.Reset();
					bSeparator = true;
					bLastPipe = true;
				}
				else
				{
					Cell.AppendChar(Character);
					if (!FChar::IsWhitespace(Character)) { bLastPipe = false; }
				}
			}
			Cells.Add(Cell.TrimStartAndEnd());
			if (bLastPipe) { Cells.Pop(); }
			if (bFirstPipe) { Cells.RemoveAt(0); }
			return bSeparator && !Cells.IsEmpty();
		}

		bool ParseAlignment(const TArray<FString>& Cells, TArray<EHorizontalAlignment>& Alignments)
		{
			Alignments.Reset();
			for (const FString& Cell : Cells)
			{
				int32 Start = Cell.StartsWith(TEXT(":")) ? 1 : 0;
				int32 End = Cell.Len() - (Cell.EndsWith(TEXT(":")) ? 1 : 0);
				if (End - Start < 3) { return false; }
				for (int32 Index = Start; Index < End; ++Index)
				{
					if (Cell[Index] != TEXT('-')) { return false; }
				}
				Alignments.Add(Start == 1 && End != Cell.Len() ? HAlign_Center
					: End != Cell.Len() ? HAlign_Right : HAlign_Left);
			}
			return true;
		}

		TSharedRef<SMultiLineEditableText> MakeText(const FString& Markdown, bool bWrap, bool bHeader = false)
		{
			TArray<TSharedRef<ITextDecorator>> Decorators;
			FSlateFontInfo Font = UACFAssistantSettings::Get().ResolvedChatFont();
			if (bHeader) { Font.TypefaceFontName = TEXT("Bold"); }
			return SNew(SMultiLineEditableText).IsReadOnly(true).AutoWrapText(bWrap).AllowContextMenu(true).TextStyle(&BodyTextStyle())
				.ClearTextSelectionOnFocusLoss(false).Font(Font).LineHeightPercentage(1.25f)
				.Marshaller(FRichTextLayoutMarshaller::Create(Decorators, GetMarkdownStyle()))
				.Text(FText::FromString(ToRichText(Markdown)));
		}
	}

	void SSelectableBody::Construct(const FArguments& InArgs)
	{
		ChildSlot [ SAssignNew(Content, SVerticalBox) ];
	}

	void SSelectableBody::SetProse(int32 Index, const FString& Markdown)
	{
		if (Blocks.IsValidIndex(Index) && Blocks[Index].Prose.IsValid())
		{
			FBlock& Block = Blocks[Index];
			if (Block.ProseText != Markdown)
			{
				Block.ProseText = Markdown;
				Block.Prose->SetText(FText::FromString(ToRichText(Markdown)));
			}
			return;
		}
		RemoveBlocksFrom(Index);
		FBlock& Block = Blocks.AddDefaulted_GetRef();
		Block.ProseText = Markdown;
		Block.Prose = MakeText(Markdown, true);
		Block.Root = Block.Prose;
		Content->AddSlot().AutoHeight() [ Block.Prose.ToSharedRef() ];
	}

	void SSelectableBody::SetTable(int32 Index, const TArray<TArray<FString>>& Rows, const TArray<EHorizontalAlignment>& Alignments)
	{
		// A streaming table only gains rows; one that lost rows or changed its columns is built again.
		if (!Blocks.IsValidIndex(Index) || !Blocks[Index].Grid.IsValid() || Blocks[Index].Alignments != Alignments || Blocks[Index].CellText.Num() > Rows.Num())
		{
			RemoveBlocksFrom(Index);
			FBlock& Created = Blocks.AddDefaulted_GetRef();
			Created.Alignments = Alignments;
			Created.Grid = SNew(SGridPanel);
			Created.Root = SNew(SScrollBox).Orientation(Orient_Horizontal) + SScrollBox::Slot() [ Created.Grid.ToSharedRef() ];
			Content->AddSlot().AutoHeight() [ Created.Root.ToSharedRef() ];
		}
		FBlock& Block = Blocks[Index];
		for (int32 Row = 0; Row < Rows.Num(); ++Row)
		{
			if (Row == Block.CellText.Num())
			{
				Block.CellText.AddDefaulted();
				Block.Cells.AddDefaulted();
			}
			for (int32 Column = 0; Column < Rows[Row].Num(); ++Column)
			{
				const FString& Text = Rows[Row][Column];
				if (Column == Block.CellText[Row].Num())
				{
					const TSharedRef<SMultiLineEditableText> Cell = MakeText(Text, false, Row == 0);
					Block.CellText[Row].Add(Text);
					Block.Cells[Row].Add(Cell);
					Block.Grid->AddSlot(Column, Row).Padding(8.0f, 4.0f).HAlign(Alignments[Column]) [ Cell ];
				}
				else if (Block.CellText[Row][Column] != Text)
				{
					Block.CellText[Row][Column] = Text;
					Block.Cells[Row][Column]->SetText(FText::FromString(ToRichText(Text)));
				}
			}
		}
	}

	void SSelectableBody::RemoveBlocksFrom(int32 Index)
	{
		while (Blocks.Num() > Index)
		{
			Content->RemoveSlot(Blocks.Last().Root.ToSharedRef());
			Blocks.Pop();
		}
	}

	void SSelectableBody::SetMarkdown(const FString& Markdown)
	{
		if (!Markdown.Contains(TEXT("|")))
		{
			SetProse(0, Markdown);
			RemoveBlocksFrom(1);
			return;
		}
		int32 BlockIndex = 0;
		TArray<FMarkdownLine> Lines;
		for (int32 Start = 0; Start < Markdown.Len();)
		{
			int32 End = Start;
			while (End < Markdown.Len() && Markdown[End] != TEXT('\r') && Markdown[End] != TEXT('\n')) { ++End; }
			const int32 TextEnd = End;
			if (End < Markdown.Len() && Markdown[End++] == TEXT('\r') && End < Markdown.Len() && Markdown[End] == TEXT('\n')) { ++End; }
			Lines.Add({Markdown.Mid(Start, TextEnd - Start), Start, End});
			Start = End;
		}
		int32 ProseStart = 0;
		TCHAR Fence = 0;
		int32 FenceLength = 0;
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex].Text;
			int32 MarkerStart = 0;
			while (MarkerStart < Line.Len() && MarkerStart < 3 && Line[MarkerStart] == TEXT(' ')) { ++MarkerStart; }
			int32 MarkerLength = 0;
			if (MarkerStart < Line.Len() && (Line[MarkerStart] == TEXT('`') || Line[MarkerStart] == TEXT('~')))
			{
				while (MarkerStart + MarkerLength < Line.Len() && Line[MarkerStart + MarkerLength] == Line[MarkerStart]) { ++MarkerLength; }
			}
			if (MarkerLength >= 3)
			{
				if (!Fence) { Fence = Line[MarkerStart]; FenceLength = MarkerLength; }
				else if (Line[MarkerStart] == Fence && MarkerLength >= FenceLength
					&& Line.Mid(MarkerStart + MarkerLength).TrimStartAndEnd().IsEmpty()) { Fence = 0; }
				continue;
			}
			if (Fence || LineIndex + 1 >= Lines.Num()) { continue; }
			TArray<FString> Header;
			TArray<FString> Delimiter;
			TArray<EHorizontalAlignment> Alignments;
			if (!SplitTableRow(Line, Header) || !SplitTableRow(Lines[LineIndex + 1].Text, Delimiter)
				|| Header.Num() != Delimiter.Num() || !ParseAlignment(Delimiter, Alignments)) { continue; }
			if (Lines[LineIndex].Start > ProseStart)
			{
				SetProse(BlockIndex++, Markdown.Mid(ProseStart, Lines[LineIndex].Start - ProseStart));
			}
			TArray<TArray<FString>> Rows;
			Rows.Add(Header);
			int32 LastLine = LineIndex + 1;
			while (LastLine + 1 < Lines.Num())
			{
				const FString NextLine = Lines[LastLine + 1].Text.TrimStart();
				if (NextLine.StartsWith(TEXT("```")) || NextLine.StartsWith(TEXT("~~~"))) { break; }
				TArray<FString> Cells;
				if (!SplitTableRow(Lines[LastLine + 1].Text, Cells) || Cells.Num() != Header.Num()) { break; }
				Rows.Add(MoveTemp(Cells));
				++LastLine;
			}
			SetTable(BlockIndex++, Rows, Alignments);
			ProseStart = Lines[LastLine].End;
			LineIndex = LastLine;
		}
		if (BlockIndex == 0 || ProseStart < Markdown.Len())
		{
			SetProse(BlockIndex++, Markdown.Mid(ProseStart));
		}
		RemoveBlocksFrom(BlockIndex);
	}

	TSharedRef<SSelectableBody> CreateSelectableBody(const FString& Markdown)
	{
		TSharedRef<SSelectableBody> Widget = SNew(SSelectableBody);
		Widget->SetMarkdown(Markdown);
		return Widget;
	}

	void SetBodyText(const TSharedRef<SSelectableBody>& Widget, const FString& Markdown)
	{
		Widget->SetMarkdown(Markdown);
	}
}
