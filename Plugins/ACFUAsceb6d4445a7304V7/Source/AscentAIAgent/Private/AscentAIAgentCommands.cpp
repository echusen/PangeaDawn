// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "AscentAIAgentCommands.h"

#define LOCTEXT_NAMESPACE "FAscentAIAgentCommands"

void FAscentAIAgentCommands::RegisterCommands()
{
    UI_COMMAND(
        OpenAIAgentTab,
        "Yarin AI",
        "Open Yarin AI",
        EUserInterfaceActionType::Button,
        FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::A));
}

#undef LOCTEXT_NAMESPACE
