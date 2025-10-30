// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 


#include "Graph/ADSDialogueNode.h"
#include "Graph/ADSDialogueResponseNode.h"
#include <GameFramework/PlayerController.h>
#include "Graph/ADSGraphEdge.h"
#include "AGSGraphNode.h"
#include <CineCameraActor.h>
#include "AGSCondition.h"
#include "ADSDialoguePartecipantComponent.h"

void UADSDialogueNode::ActivateNode()
{
    Super::ActivateNode();

	if (partecipant ) {

        partecipant->SetCamera(CameraSettings);
	}
	
}

UADSDialogueNode::UADSDialogueNode()
{
#if WITH_EDITOR
	BackgroundColor = FLinearColor::Black;
	ContextMenuName = FText::FromString("Dialogue Node");
#endif
}

TArray<UADSDialogueResponseNode*>  UADSDialogueNode::GetAllValidAnswers( APlayerController* inController)
{
	TArray<UADSDialogueResponseNode*>  outResponses;
	for ( UAGSGraphNode* child : ChildrenNodes)
	{
		UADSDialogueResponseNode* response = Cast<UADSDialogueResponseNode>(child);
		if (response && response->CanBeActivated(inController))
		{
			UADSGraphEdge* edge = Cast< UADSGraphEdge>(GetEdge(response));
			if (edge && edge->CanBeActivated(inController)) {
				outResponses.Add(response);
			}

		}
	}
	return outResponses;
}

bool UADSDialogueNode::CanBeActivated(APlayerController* inController)
{
    controller = inController;
    for (UAGSCondition* cond : ActivationConditions) {
        if (cond && !cond->VerifyForNode(inController, this))
            return false;
    }
    return true;
}
