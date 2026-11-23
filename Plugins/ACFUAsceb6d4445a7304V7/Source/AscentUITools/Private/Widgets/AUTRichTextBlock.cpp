// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Widgets/AUTRichTextBlock.h"
#include "AUTThemeDataAsset.h"
#include "AUTUIFunctionLibrary.h"

void UAUTRichTextBlock::PostLoad()
{
	Super::PostLoad();

	if (!StyleTag.IsValid()) {
		StyleTag = UAUTThemeDataAsset::GetLegacyTitleStyleTag(StyleIndex);
	}
}

void UAUTRichTextBlock::SynchronizeProperties()
{
	UpdateStyle();
    
	Super::SynchronizeProperties();
}

void UAUTRichTextBlock::SetStyle(FGameplayTag inStyleTag)
{
	StyleTag = inStyleTag;
	UpdateStyle();
}

void UAUTRichTextBlock::UpdateStyle()
{
	FAUTBaseTextStyle outBack;
	if (bUseStyle && UAUTUIFunctionLibrary::TryGetTitleStyle(StyleTag, outBack)) {
		outBack.Normal.Size = TextSize;
		SetDefaultFont(outBack.Normal);
		SetDefaultColorAndOpacity(outBack.NormalColor);
	}
}

void UAUTRichTextBlock::SetFontSize(int32 inFontSize)
{
	TextSize = inFontSize;
	UpdateStyle();
}
