// // Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.


#include "Graph/ADSWorldDialogue.h"
#include "Graph/ADSDialogueNode.h"


UADSWorldDialogue::UADSWorldDialogue()
{
	NodeType = UADSDialogueNode::StaticClass();
	UseDefaultCameraSettings = false;
}
