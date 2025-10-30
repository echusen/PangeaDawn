// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "ADSTypes.h"
#include "CoreMinimal.h"
#include "Graph/ADSGraphNode.h"

#include "ADSDialogueNode.generated.h"

class ACineCameraActor;
class ATargetPoint;
class UAGSCondition;

/**
 *
 */
UCLASS()
class ASCENTDIALOGUESYSTEM_API UADSDialogueNode : public UADSGraphNode {
    GENERATED_BODY()

protected:
    // The camera used to frame the speaking character during dialogue
    UPROPERTY(EditAnywhere, Category = "ADS|Camera")
    FDialogueCinematic CameraSettings;



    virtual void ActivateNode() override;

public:
    UADSDialogueNode();

    UFUNCTION(BlueprintCallable, Category = ADS)
    TArray<class UADSDialogueResponseNode*> GetAllValidAnswers(APlayerController* inController);

    virtual bool CanBeActivated(APlayerController* inController) override;
};
