// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFFreeClimbJumpMovementNotify.h"

#include "ACFFreeClimbComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UACFFreeClimbJumpMovementNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
    if (UACFFreeClimbComponent* ClimbComponent = Owner ? Owner->FindComponentByClass<UACFFreeClimbComponent>() : nullptr)
    {
        ClimbComponent->StartJumpMovement();
    }
}

FString UACFFreeClimbJumpMovementNotify::GetNotifyName_Implementation() const
{
    return TEXT("ACF Start Jump Movement");
}
