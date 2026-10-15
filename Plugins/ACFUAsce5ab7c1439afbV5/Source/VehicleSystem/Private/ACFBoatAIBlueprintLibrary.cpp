// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFBoatAIBlueprintLibrary.h"
#include "BehaviorTree/BehaviorTree.h"

void UACFBoatAIBlueprintLibrary::ResetBehaviorTreeEditorGraph(UBehaviorTree* BehaviorTree)
{
#if WITH_EDITORONLY_DATA
    if (BehaviorTree) {
        BehaviorTree->BTGraph = nullptr;
        BehaviorTree->MarkPackageDirty();
    }
#endif
}
