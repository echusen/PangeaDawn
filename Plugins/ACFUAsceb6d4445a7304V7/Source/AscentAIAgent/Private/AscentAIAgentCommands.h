// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "AscentAIAgentStyle.h"

class FAscentAIAgentCommands : public TCommands<FAscentAIAgentCommands>
{
public:
    FAscentAIAgentCommands()
        : TCommands<FAscentAIAgentCommands>(
            TEXT("AscentAIAgent"),
            NSLOCTEXT("Contexts", "AscentAIAgent", "Yarin AI"),
            NAME_None,
            FAscentAIAgentStyle::GetStyleSetName())
    {
    }

    virtual void RegisterCommands() override;

    TSharedPtr<FUICommandInfo> OpenAIAgentTab;
};
