// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.


#include "AUTDeveloperSettings.h"
#include "AUTThemeDataAsset.h"

 UAUTThemeDataAsset* UAUTDeveloperSettings::GetTheme() const
{
 	return Cast<UAUTThemeDataAsset>(ThemesAsset.TryLoad());
}

TSoftObjectPtr<UWorld> UAUTDeveloperSettings::GetDefaultMenuMapRef() const
{
    TSoftObjectPtr<UWorld> newWorld;
    newWorld = DefaultMenuMap;
    return newWorld;
}

TSoftObjectPtr<UWorld> UAUTDeveloperSettings::GetDefaultNewGameMapRef() const
{
    TSoftObjectPtr<UWorld> newWorld;
    newWorld = DefaultNewGameMap;
    return newWorld;
}
