// Copyright Dominik Peacock. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"



class FACFGASCommands : public TCommands<FACFGASCommands>
{
public:

    FACFGASCommands()
        : TCommands<FACFGASCommands>(TEXT("GAS Editor Commands"), NSLOCTEXT("Contexts", "AscentGASEditorCommands", "Ascent GAS Editor Commands"), NAME_None, FAppStyle::GetAppStyleSetName())
    {}
    void RegisterCommands() override;

    TSharedPtr<FUICommandInfo> OpenAttributeCreator;
};


