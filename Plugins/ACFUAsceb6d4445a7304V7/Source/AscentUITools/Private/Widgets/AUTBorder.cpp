// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.


#include "Widgets/AUTBorder.h"
#include "AUTThemeDataAsset.h"
#include "AUTUIFunctionLibrary.h"

void UAUTBorder::PostLoad()
{
    Super::PostLoad();

    if (!StyleTag.IsValid()) {
        StyleTag = UAUTThemeDataAsset::GetLegacyBackgroundStyleTag(StyleIndex);
    }
}


void UAUTBorder::SynchronizeProperties()
{
    SetStyle(StyleTag);
	Super::SynchronizeProperties();
}

void UAUTBorder::SetStyle(FGameplayTag inStyleTag) {
        StyleTag = inStyleTag;
        FSlateBrush outBack;
        if (UAUTUIFunctionLibrary::TryGetBackgroundStyle(StyleTag, outBack) && bUseStyle) {
            Background = outBack;
        }
}
