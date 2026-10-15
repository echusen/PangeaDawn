// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ACFBoatAIBlueprintLibrary.generated.h"

class UBehaviorTree;

UCLASS()
class VEHICLESYSTEM_API UACFBoatAIBlueprintLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()

public:
    /**
     * Editor utility: clears the editor-only node graph of a BehaviorTree asset so that,
     * on next open, the BT editor regenerates it from the runtime tree. Needed for behavior
     * trees whose runtime tree was authored programmatically — otherwise a stale graph
     * recompiles over the runtime nodes when the asset loads. No-op in packaged builds.
     */
    UFUNCTION(BlueprintCallable, Category = ACF)
    static void ResetBehaviorTreeEditorGraph(UBehaviorTree* BehaviorTree);
};
