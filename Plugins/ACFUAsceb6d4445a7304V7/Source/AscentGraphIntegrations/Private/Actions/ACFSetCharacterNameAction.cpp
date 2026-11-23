// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFSetCharacterNameAction.h"

#include "ADSDialoguePartecipantComponent.h"
#include "Actors/ACFCharacter.h"
#include "GameFramework/Controller.h"
#include "Graph/ADSGraphNode.h"
#include "Logging.h"

UADSDialoguePartecipantComponent* UACFSetCharacterNameAction::GetNodeParticipant() const
{
    if (const UADSGraphNode* dialogueNode = Cast<UADSGraphNode>(NodeOwner.Get())) {
        return dialogueNode->GetDialogueParticipant();
    }

    return nullptr;
}

AActor* UACFSetCharacterNameAction::GetTargetActor() const
{
    if (const UADSDialoguePartecipantComponent* nodeParticipant = GetNodeParticipant()) {
        if (AActor* participantOwner = nodeParticipant->GetOwner()) {
            return participantOwner;
        }
    }

    if (!bFallbackToGraphOwner) {
        return nullptr;
    }

    AActor* graphOwner = GetGraphOwnerActor();
    if (const AController* ownerController = Cast<AController>(graphOwner)) {
        if (APawn* possessedPawn = ownerController->GetPawn()) {
            return possessedPawn;
        }
    }

    return graphOwner;
}

AACFCharacter* UACFSetCharacterNameAction::GetTargetCharacter() const
{
    return Cast<AACFCharacter>(GetTargetActor());
}

bool UACFSetCharacterNameAction::ApplyCharacterName()
{
    bool bRenamedSomething = false;

    if (AACFCharacter* targetCharacter = GetTargetCharacter()) {
        targetCharacter->SetCharacterName(NewCharacterName);
        bRenamedSomething = true;
    }

    if (bAlsoRenameDialogueParticipant) {
        if (UADSDialoguePartecipantComponent* nodeParticipant = GetNodeParticipant()) {
            nodeParticipant->SetParticipantName(NewCharacterName);
            bRenamedSomething = true;
        }
    }

    if (!bRenamedSomething) {
        UE_LOG(ACFLog, Warning,
            TEXT("SetACFCharacterName: no ACF Character assigned to this node, name not applied - %s"), *GetNameSafe(this));
    }

    return bRenamedSomething;
}

void UACFSetCharacterNameAction::ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner)
{
    Super::ExecuteAction_Implementation(playerController, nodeOwner);

    ApplyCharacterName();
}
