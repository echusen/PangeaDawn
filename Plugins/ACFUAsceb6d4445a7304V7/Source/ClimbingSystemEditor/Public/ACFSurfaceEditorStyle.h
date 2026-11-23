// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class CLIMBINGSYSTEMEDITOR_API FACFSurfaceEditorStyle
{
public:
    static void Initialize();
    static void Shutdown();

    static const ISlateStyle& Get();
    static FName GetStyleSetName();

private:
    static TSharedPtr<FSlateStyleSet> StyleSet;
};
