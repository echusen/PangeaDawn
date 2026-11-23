// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "Actions/ACFEndWorldDialogueAction.h"

#include "ADSDialogueSubsystem.h"
#include "AGSGraph.h"
#include "AGSGraphNode.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Graph/ADSWorldDialogue.h"
#include "TimerManager.h"

void UACFEndWorldDialogueAction::ExecuteAction_Implementation(APlayerController* playerController, UAGSGraphNode* nodeOwner)
{
    Super::ExecuteAction_Implementation(playerController, nodeOwner);
    EndWorldDialogue();
}

bool UACFEndWorldDialogueAction::EndWorldDialogue()
{
    UADSWorldDialogue* OwningDialogue = GetOwningWorldDialogue();
    const UADSDialogueSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UADSDialogueSubsystem>() : nullptr;

    const bool bHasDialogueToEnd = (IsValid(OwningDialogue) && OwningDialogue->IsDialogueStarted())
        || (Subsystem && Subsystem->IsPlayingWorldDialogue());

    if (!bHasDialogueToEnd) {
        UE_LOG(LogTemp, Warning, TEXT("ACF End World Dialogue: no world dialogue is playing."));
        return false;
    }

    if (bEndScheduled) {
        return true;
    }

    // Activation actions run before the node plays its line, moves the camera and arms its auto-progress
    // timer. Ending here would leave all of that running, so the end waits for the node to finish activating.
    UWorld* World = GetWorld();
    if (!World && NodeOwner) {
        World = NodeOwner->GetWorld();
    }

    if (!World) {
        EndWorldDialogueNow();
        return true;
    }

    bEndScheduled = true;
    World->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateUObject(this, &UACFEndWorldDialogueAction::EndWorldDialogueNow));
    return true;
}

UADSWorldDialogue* UACFEndWorldDialogueAction::GetOwningWorldDialogue() const
{
    return NodeOwner ? Cast<UADSWorldDialogue>(NodeOwner->GetGraph()) : nullptr;
}

void UACFEndWorldDialogueAction::EndWorldDialogueNow()
{
    bEndScheduled = false;

    if (UADSWorldDialogue* OwningDialogue = GetOwningWorldDialogue()) {
        // The dialogue may have ended on its own while the end was pending.
        if (OwningDialogue->IsDialogueStarted()) {
            OwningDialogue->EndDialogue();
        }
        return;
    }

    // This graph is not a world dialogue: end whatever world dialogue is playing.
    if (UADSDialogueSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UADSDialogueSubsystem>() : nullptr) {
        Subsystem->StopAllWorldDialogues();
    }
}
