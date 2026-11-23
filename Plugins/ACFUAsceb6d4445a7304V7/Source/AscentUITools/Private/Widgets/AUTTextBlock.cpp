// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Widgets/AUTTextBlock.h"
#include "AUTThemeDataAsset.h"
#include "AUTUIFunctionLibrary.h"

void UAUTTextBlock::PostLoad()
{
    Super::PostLoad();

    if (!StyleTag.IsValid()) {
        StyleTag = UAUTThemeDataAsset::GetLegacyTitleStyleTag(StyleIndex);
    }
}

void UAUTTextBlock::SynchronizeProperties()
{
    UpdateStyle();
    
    Super::SynchronizeProperties();
}

void UAUTTextBlock::SetStyle(FGameplayTag inStyleTag)
{
    StyleTag = inStyleTag;
    UpdateStyle();
}

void UAUTTextBlock::UpdateStyle()
{
    FAUTBaseTextStyle outBack;
    if (bUseStyle && UAUTUIFunctionLibrary::TryGetTitleStyle(StyleTag, outBack)) {
        outBack.Normal.Size = TextSize;
        SetFont(outBack.Normal);
        SetColorAndOpacity(outBack.NormalColor);
    }
}

void UAUTTextBlock::SetFontSize(int32 inFontSize)
{
    TextSize = inFontSize;
    UpdateStyle();
}
