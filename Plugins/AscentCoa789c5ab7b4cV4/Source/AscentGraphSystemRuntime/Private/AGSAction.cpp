// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved. 


#include "AGSAction.h"

void UAGSAction::Execute(class APlayerController* playerController, UAGSGraphNode* nodeOwner)
{
	Controller = playerController;
	ExecuteAction(Controller, nodeOwner);
}

void UAGSAction::ExecuteAction_Implementation(class APlayerController* playerController, UAGSGraphNode* nodeOwner)
{

}

UWorld* UAGSAction::GetWorld() const
{
	if(Controller){
		return Controller->GetWorld();
	}
	return nullptr;
}
