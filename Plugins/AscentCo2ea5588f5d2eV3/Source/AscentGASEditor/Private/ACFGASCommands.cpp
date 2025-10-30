// Copyright Dominik Peacock. All rights reserved.

#include "ACFGASCommands.h"



#define LOCTEXT_NAMESPACE "FACFGASCommands"

void FACFGASCommands::RegisterCommands()
{
    UI_COMMAND(OpenAttributeCreator, "Attribute Creator", "Create new Attributed in Editor",
        EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
