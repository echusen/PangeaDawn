// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "SCommonEditorViewportToolbarBase.h"

class CLIMBINGSYSTEMEDITOR_API SACFSurfaceViewportToolbar : public SCommonEditorViewportToolbarBase
{
public:
    virtual void FillShowFlagsMenu(UToolMenu* InMenu) const override;
};
