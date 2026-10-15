// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ACFFreeClimbJumpMovementNotify.generated.h"

/** Place on the climb jump animation where the character should leave its grip. Calls Start Climb Jump Movement
 * on the owner's free climb component, which waits for it when Jump Movement Waits For Notify is enabled. */
UCLASS(BlueprintType, meta = (DisplayName = "ACF Free Climb Start Jump Movement"))
class CLIMBINGSYSTEM_API UACFFreeClimbJumpMovementNotify : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
    virtual FString GetNotifyName_Implementation() const override;
};
