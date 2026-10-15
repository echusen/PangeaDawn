// Fill out your copyright notice in the Description page of Project Settings.


#include "BAGraphUtils.h"

int FBAGraphUtils::StraightenPin(FBAPinPtr SourcePin, FBAPinPtr PinToAlign)
{
	if (!SourcePin || !PinToAlign)
	{
		return 0;
	}

	FBANodePtr NodeToAlign = PinToAlign->GetOwningNode();
	const int DeltaY = SourcePin->GetPos().Y - PinToAlign->GetPos().Y;
	NodeToAlign->NodePosY += DeltaY;
	return DeltaY;
}
