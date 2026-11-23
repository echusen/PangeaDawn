// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFComboComponent.h"
#include "Components/ACFAbilitySystemComponent.h"
#include "Graph/ACFComboGraph.h"
#include "Graph/ACFComboNode.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include <EnhancedInputComponent.h>
#include <EnhancedPlayerInput.h>
#include <InputAction.h>
#include <Net/UnrealNetwork.h>
#include <UObject/CoreNet.h>

// Sets default values for this component's properties
UACFComboComponent::UACFComboComponent()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    bIsInputBufferEnabled = false;
    bIsPerformingCombo = false;
    // ...
}

void UACFComboComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UACFComboComponent, LastTagInput);
    DOREPLIFETIME(UACFComboComponent, bIsPerformingCombo);
}

void UACFComboComponent::StartCombo(UACFComboGraph* comboToStart, const FGameplayTag& triggeringAction)
{
    if (IsExecutingAnyCombo()) {
        StopCurrentCombo();
    }
    if (comboToStart) {
        FActionState action;
        if (actionsComp->GetAbilityByTag(triggeringAction)) {
            // Duplicate the asset so each character has its own independent graph state
            currentCombo = DuplicateObject(comboToStart, GetOwner());
            currentCombo->SetCharacterOwner(Cast<ACharacter>(GetOwner()));
            currentCombo->StartCombo(triggeringAction);
            bIsPerformingCombo = currentCombo->IsActive();
        }
    }
}

void UACFComboComponent::StopCombo(UACFComboGraph* comboToStop)
{
    // Compare by class: currentCombo is a DuplicateObject of comboToStop (same class, different pointer)
    if (currentCombo && comboToStop && currentCombo->GetClass() == comboToStop->GetClass()) {
        StopCurrentCombo();
    }
}

void UACFComboComponent::SetInputBufferOpened(bool bEnabled)
{
    bIsInputBufferEnabled = bEnabled;
    if (bEnabled) {
        // [Multi-input combo fix] Latch that this node opened for transition evaluation. Unlike
        // bIsInputBufferEnabled it survives the SetInputBufferOpened(false) at ability end, and is
        // reset per node in BeginComboNode() — so TryConsumeBufferedTransition still runs at step-end.
        bTransitionWindowWasOpenedForCurrentNode = true;
    }
}

bool UACFComboComponent::IsExecutingCombo(UACFComboGraph* combo) const
{
    // Compare by class: currentCombo is a DuplicateObject of the source asset (same class, different pointer)
    return IsExecutingAnyCombo() && currentCombo && combo && currentCombo->GetClass() == combo->GetClass();
}

bool UACFComboComponent::IsExecutingAnyCombo() const
{
    return bIsPerformingCombo;
}

// Called when the game starts
void UACFComboComponent::BeginPlay()
{
    Super::BeginPlay();

    actionsComp = GetOwner()->FindComponentByClass<UACFAbilitySystemComponent>();
    if (!actionsComp) {
        UE_LOG(LogTemp, Error, TEXT("Missing ACTIONS Comp!  - UACFComboComponent"));
    }
}

void UACFComboComponent::SetupPlayerInputComponent(UEnhancedInputComponent* EnhancedInputComponent)
{
    if (EnhancedInputComponent != nullptr) {
        for (const TPair<UInputAction*, FGameplayTag>& Pair : ComboInputs) {
            UInputAction* input = Pair.Key;

            EnhancedInputComponent->BindAction(input, ETriggerEvent::Started, this,
                &UACFComboComponent::HandleComboInputReceived, input);
        }
    } else {
        UE_LOG(LogTemp, Warning, TEXT("Missing Enhanced Input Comp!  - UACFComboComponent"));
    }
}

void UACFComboComponent::HandleComboInputReceived(UInputAction* input)
{
    const FGameplayTag* foundTag = ComboInputs.Find(input);
    // pre setting it locally to avoid network delays, server check will eventually rollback it in
    // ability end.
    // [Multi-input combo fix] The same-tag dedupe (GetLastTagInput() != *foundTag) was removed: it
    // discarded legitimate repeated presses on a same-key chain. The node-scoped buffer below now
    // handles repeats and timing correctly.
    if (foundTag && Internal_InputReceived(*foundTag)) {
        SendInputReceived(*foundTag);
    }
}

void UACFComboComponent::SendInputReceived_Implementation(const FGameplayTag& tagInput)
{
    Internal_InputReceived(tagInput);
}

void UACFComboComponent::ClearInputTag()
{
    LastTagInput = FGameplayTag();
}

void UACFComboComponent::StopCurrentCombo()
{
    Internal_StopCombo(); // Sempre sul server

    // Notifica l'owner client di fermare la combo (solo se non siamo in single player)
    if (GetOwner()->GetNetMode() != NM_Standalone && !GetOwner()->HasLocalNetOwner()) {
        ClientStopCombo();
    }
}

void UACFComboComponent::Internal_StopCombo()
{
    if (currentCombo) {
        currentCombo->StopCombo();
        currentCombo = nullptr;
        bIsPerformingCombo = false;
        ClearInputTag();
    }
    // [Multi-input combo fix] reset buffer + continuation state whenever the combo ends
    ClearBufferedInputs();
    bTransitionWindowWasOpenedForCurrentNode = false;
    AuthorizedNextNodeAction = FGameplayTag();
}

bool UACFComboComponent::Internal_InputReceived(const FGameplayTag& tagInput)
{
    // [Multi-input combo fix] Capture the press while a combo is running, regardless of whether the
    // buffer window is momentarily open. Holding it in a node-scoped buffer (instead of a single slot
    // cleared each node) is what lets an early press, or a press in the node-handoff gap, survive to
    // the correct step-end.
    if (!bIsPerformingCombo || !currentCombo || !currentCombo->GetCurrentComboNode()) {
        return false;
    }

    FACFBufferedComboInput NewInput;
    NewInput.InputTag = tagInput;
    NewInput.ReceivedWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    NewInput.CapturedGraph = currentCombo;
    NewInput.CapturedNode = currentCombo->GetCurrentComboNode();
    PendingComboInputs.Add(tagInput, NewInput);
    LastTagInput = tagInput; // kept for Blueprint/HUD compatibility
    return true;
}

void UACFComboComponent::ClientStopCombo_Implementation()
{
    Internal_StopCombo();
}

// ---------------------------------------------------------------------------------------------------
// [Multi-input combo fix] node-scoped input buffer + single-use continuation authorization
// ---------------------------------------------------------------------------------------------------

void UACFComboComponent::BeginComboNode()
{
    // A new node is now current; require the buffer window to be (re)opened for it.
    bTransitionWindowWasOpenedForCurrentNode = false;
}

void UACFComboComponent::ClearBufferedInputs()
{
    PendingComboInputs.Reset();
    LastTagInput = FGameplayTag();
}

bool UACFComboComponent::TryConsumeBufferedTransition(const ACharacter* Character, FGameplayTag& OutConsumedInput)
{
    OutConsumedInput = FGameplayTag();
    if (!currentCombo || !currentCombo->GetCurrentComboNode() || !bTransitionWindowWasOpenedForCurrentNode) {
        return false;
    }

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    UACFComboNode* CurrentNode = currentCombo->GetCurrentComboNode();
    const TArray<FGameplayTag> ValidInputs = currentCombo->GetValidInputsForCurrentNode();

    // Evaluate oldest-first so a genuinely earlier press wins.
    TArray<FACFBufferedComboInput> Candidates;
    PendingComboInputs.GenerateValueArray(Candidates);
    Candidates.Sort([](const FACFBufferedComboInput& A, const FACFBufferedComboInput& B) {
        return A.ReceivedWorldTime < B.ReceivedWorldTime;
    });

    for (const FACFBufferedComboInput& Candidate : Candidates) {
        // Node-scope: a press only advances the exact node (and combo run) it was made during.
        if (Candidate.CapturedGraph.Get() != currentCombo || Candidate.CapturedNode.Get() != CurrentNode) {
            PendingComboInputs.Remove(Candidate.InputTag);
            continue;
        }

        // Expiry clock starts at the FIRST step-end evaluation (not at press time), so an early press
        // in a long montage is never aged out before its own node's evaluation.
        if (FACFBufferedComboInput* Live = PendingComboInputs.Find(Candidate.InputTag)) {
            if (!Live->bHasReachedFirstEvaluationPoint) {
                Live->bHasReachedFirstEvaluationPoint = true;
                Live->ExpiryClockStartTime = Now;
            } else if (ComboInputBufferDuration > 0.f
                && Now - Live->ExpiryClockStartTime > ComboInputBufferDuration) {
                PendingComboInputs.Remove(Candidate.InputTag);
                continue;
            }
        }

        // Must correspond to an actual outgoing edge on the current node.
        if (!ValidInputs.Contains(Candidate.InputTag)) {
            PendingComboInputs.Remove(Candidate.InputTag);
            continue;
        }

        if (currentCombo->PerformTransition(Candidate.InputTag, Character)) {
            OutConsumedInput = Candidate.InputTag;
            LastTagInput = Candidate.InputTag;
            PendingComboInputs.Reset(); // one transition per step-end
            return true;
        }
        PendingComboInputs.Remove(Candidate.InputTag);
    }
    return false;
}

void UACFComboComponent::AuthorizeNextNodeActivation(const FGameplayTag& ActionTag)
{
    AuthorizedNextNodeAction = ActionTag;
}

bool UACFComboComponent::IsNextNodeActivationAuthorized(const FGameplayTag& ActionTag) const
{
    return AuthorizedNextNodeAction.IsValid() && AuthorizedNextNodeAction == ActionTag;
}

bool UACFComboComponent::ConsumeNextNodeActivationAuthorization(const FGameplayTag& ActionTag)
{
    if (AuthorizedNextNodeAction.IsValid() && AuthorizedNextNodeAction == ActionTag) {
        AuthorizedNextNodeAction = FGameplayTag();
        return true;
    }
    return false;
}
