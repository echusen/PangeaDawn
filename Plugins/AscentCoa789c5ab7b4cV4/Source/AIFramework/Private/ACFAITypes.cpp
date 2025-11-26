// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "ACFAITypes.h"
#include "ACFAIController.h"
#include "Actors/ACFCharacter.h"

FAIAgentsInfo::FAIAgentsInfo(AACFCharacter* inChar)
{
    AICharacter = inChar;
    if (inChar) {
        characterClass = inChar->GetClass();
    }
}

TObjectPtr<class AACFAIController> FAIAgentsInfo::GetController() const
{
    if (AICharacter) {
        return Cast<AACFAIController>(AICharacter->GetController());

    }

    return nullptr;
}
