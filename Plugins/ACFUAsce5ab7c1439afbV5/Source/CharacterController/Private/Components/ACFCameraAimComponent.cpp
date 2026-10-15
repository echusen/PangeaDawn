// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Components/ACFCameraAimComponent.h"

#include "Components/ACFCharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ACFCameraAimComponent)

UACFCameraAimComponent::UACFCameraAimComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UACFCameraAimComponent::BeginPlay()
{
    Super::BeginPlay();

    cachedMesh = ResolveMesh();
    RefreshTickPrerequisites();
}

void UACFCameraAimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // The controller can change under us on possession and respawn, and the whole point of this
    // component is to read the rotation the current one just produced.
    RefreshTickPrerequisites();
    UpdateViewRotation();
}

void UACFCameraAimComponent::SetCameraAimEnabled(bool bInEnabled)
{
    if (bCameraAimActive == bInEnabled) {
        return;
    }

    if (bInEnabled) {
        // Anchoring the pose to a view nobody is looking through only costs performance, and on a
        // simulated proxy the control rotation is not the one that character is really aiming with.
        const APawn* pawn = Cast<APawn>(GetOwner());
        if (!pawn || !pawn->IsLocallyControlled()) {
            return;
        }
    }

    bCameraAimActive = bInEnabled;
    SetComponentTickEnabled(bInEnabled);

    ApplyMeshVisibility(bInEnabled);
    ApplyRotationMode(bInEnabled);

    if (bInEnabled) {
        RefreshTickPrerequisites();
        UpdateViewRotation();
    }

    OnCameraAimChanged.Broadcast(bInEnabled);
}

void UACFCameraAimComponent::UpdateViewRotation()
{
    const APawn* pawn = Cast<APawn>(GetOwner());
    if (!pawn) {
        return;
    }

    // Same source UACFAnimInstance uses for its own aim rotation: the control rotation is only
    // meaningful locally, and a proxy has the replicated aim rotation instead.
    cachedViewRotation = pawn->IsLocallyControlled()
        ? pawn->GetControlRotation()
        : pawn->GetBaseAimRotation();
}

void UACFCameraAimComponent::RefreshTickPrerequisites()
{
    AActor* owner = GetOwner();
    APawn* pawn = Cast<APawn>(owner);
    AController* controller = pawn ? pawn->GetController() : nullptr;

    if (controller != cachedController.Get()) {
        if (AController* previousController = cachedController.Get()) {
            PrimaryComponentTick.RemovePrerequisite(previousController, previousController->PrimaryActorTick);
        }

        cachedController = controller;

        if (controller) {
            PrimaryComponentTick.AddPrerequisite(controller, controller->PrimaryActorTick);
        }
    }

    // Movement first, so the rotation read here is the one the pawn ends the frame with.
    if (UMovementComponent* movementComp = owner ? owner->FindComponentByClass<UMovementComponent>() : nullptr) {
        PrimaryComponentTick.AddPrerequisite(movementComp, movementComp->PrimaryComponentTick);
    }

    if (USkeletalMeshComponent* mesh = cachedMesh.Get()) {
        mesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
    }
}

void UACFCameraAimComponent::ApplyMeshVisibility(bool bInEnabled)
{
    USkeletalMeshComponent* mesh = cachedMesh.Get();
    if (!mesh) {
        return;
    }

    for (const FName& boneName : BonesToHideWhenActive) {
        if (boneName == NAME_None) {
            continue;
        }

        if (bInEnabled) {
            // PBO_None on purpose: terming the physics body would leave the ragdoll broken on death.
            mesh->HideBoneByName(boneName, PBO_None);
        } else {
            mesh->UnHideBoneByName(boneName);
        }
    }
}

void UACFCameraAimComponent::ApplyRotationMode(bool bInEnabled)
{
    if (!bForceStrafingWhenActive) {
        return;
    }

    AActor* owner = GetOwner();
    UACFCharacterMovementComponent* movementComp = owner ? owner->FindComponentByClass<UACFCharacterMovementComponent>() : nullptr;
    if (!movementComp) {
        // Not an ACF character: whoever owns the rotation mode here has to point the body themselves.
        return;
    }

    if (bInEnabled) {
        // Requested rather than current: in first person the current mode is a lock, not what to restore.
        previousRotationMode = movementComp->GetRequestedRotationMode();
        bHasStoredRotationMode = true;
        movementComp->SetRotationMode(ERotationMode::EStrafing);
    } else if (bHasStoredRotationMode) {
        movementComp->SetRotationMode(previousRotationMode);
        bHasStoredRotationMode = false;
    }
}

USkeletalMeshComponent* UACFCameraAimComponent::ResolveMesh() const
{
    AActor* owner = GetOwner();
    if (!owner) {
        return nullptr;
    }

    // Tagged first: a pawn carrying more than one skeletal mesh has to be able to say which one the
    // node runs on, the same way the sample's Blueprint component picks its animated mesh.
    if (AnimatedMeshTag != NAME_None) {
        for (UActorComponent* component : owner->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), AnimatedMeshTag)) {
            if (USkeletalMeshComponent* taggedMesh = Cast<USkeletalMeshComponent>(component)) {
                return taggedMesh;
            }
        }
    }

    if (const ACharacter* character = Cast<ACharacter>(owner)) {
        if (USkeletalMeshComponent* characterMesh = character->GetMesh()) {
            return characterMesh;
        }
    }

    return owner->FindComponentByClass<USkeletalMeshComponent>();
}
