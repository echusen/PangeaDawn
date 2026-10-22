#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/LatentActionManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// Private teardown implementation, not a gameplay API. Actor ownership must be
// established by the caller before invoking this on its own presentation pawn.
namespace PangeaMiningPrivate
{
inline void RetirePresentationActor(AActor* Actor)
{
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return;
    // The Simple template defers initialization through DelayUntilNextTick on
    // the AnimInstance. Mesh unregistration alone does NOT cancel latent actions;
    // retire that object's pending work before its CharacterOwner becomes garbage.
    auto CancelLatentInitialization = [Actor](UAnimInstance* Instance)
    {
        if (Instance && Actor->GetWorld())
            Actor->GetWorld()->GetLatentActionManager().RemoveActionsForObject(Instance);
    };
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        CancelLatentInitialization(Mesh->GetAnimInstance());
        const USkeletalMeshComponent* ReadOnlyMesh = Mesh; // The public accessor is const in UE 5.8.
        for (UAnimInstance* Linked : ReadOnlyMesh->GetLinkedAnimInstances()) CancelLatentInitialization(Linked);
        CancelLatentInitialization(Mesh->GetPostProcessInstance());
        Mesh->SetComponentTickEnabled(false);
        if (Mesh->IsRegistered()) Mesh->UnregisterComponent();
    }
    Actor->Destroy();
}
}
