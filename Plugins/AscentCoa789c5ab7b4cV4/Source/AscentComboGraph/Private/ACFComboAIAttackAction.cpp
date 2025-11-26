#include "ACFComboAIAttackAction.h"
#include "ACFComboComponent.h"
#include "Graph/ACFComboGraph.h"
#include "InputAction.h"

UACFComboAIAttackAction::UACFComboAIAttackAction()
{
    // Constructor logic if needed
}

void UACFComboAIAttackAction::OnActionStarted_Implementation()
{
    // Ensure the combo graph is marked as AI-controlled
    if (Combo) {
        Combo->SetIsAI(true);
    }
    comboComponent = GetCharacterOwner()->FindComponentByClass<UACFComboComponent>();

    Super::OnActionStarted_Implementation();
}

void UACFComboAIAttackAction::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // For AI combos, DON'T close the buffer yet - we need it open for AI input simulation
    if (Combo && comboComponent && Combo->IsAI() && comboComponent->IsExecutingAnyCombo()) {
        node = Combo->GetCurrentComboNode();
        // Skip buffer closing - let it stay open for AI
        // Call grandparent's EndAbility (skip parent's buffer-closing logic)
        UACFActionAbility::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
    } else {
        // For non-AI or ended combos, use parent behavior (close buffer)
        UACFComboAttackAction::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
    }
}

void UACFComboAIAttackAction::OnActionEnded_Implementation()
{
    if (comboComponent && node) {
        if (Combo && Combo->IsAI() && comboComponent->IsExecutingAnyCombo()) {
            // CRITICAL: Send AI input NOW, before parent checks HasPendingInput()
            SimulateAIInput();
            // Close buffer after input is sent
            comboComponent->SetInputBufferOpened(false);
        }
        // NOW call parent - it will see the pending input we just sent
        Super::OnActionEnded_Implementation();
    } else {
        Super::OnActionEnded_Implementation();
    }
}

void UACFComboAIAttackAction::SimulateAIInput()
{
    if (!Combo) {
        UE_LOG(LogTemp, Warning, TEXT("SimulateAIInput: Combo graph is not valid"));
        return;
    }

    // Extract valid inputs for the current node
    TArray<FGameplayTag> ValidInputs = Combo->GetValidInputsForCurrentNode();

    if (ValidInputs.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("SimulateAIInput: No valid inputs for the current node"));
        return;
    }

    // Calculate total probability and collect valid probabilities
    float TotalProbability = 0.0f;
    TArray<TPair<FGameplayTag, float>> ValidProbabilities;

    for (const FGameplayTag& Input : ValidInputs) {
        float Probability = AIInputProbabilities.Contains(Input) ? AIInputProbabilities[Input] : 0.0f;
        if (Probability > 0.0f) {
            TotalProbability += Probability;
            ValidProbabilities.Add(TPair<FGameplayTag, float>(Input, Probability));
        }
    }

    // If no valid probabilities, log warning and return
    if (ValidProbabilities.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("SimulateAIInput: No valid probabilities found"));
        return;
    }

    // Generate random value based on total probability
    const float RandomValue = FMath::FRand() * TotalProbability;
    float CurrentSum = 0.0f;

    // Select input based on normalized probabilities
    for (const TPair<FGameplayTag, float>& InputProb : ValidProbabilities) {
        CurrentSum += InputProb.Value;
        if (RandomValue <= CurrentSum) {

            comboComponent->SendInputReceived(InputProb.Key);
            return;
        }
    }

    // Fallback - use the last valid input (should rarely happen due to floating point precision)
    if (ValidProbabilities.Num() > 0) {
        auto& LastInput = ValidProbabilities.Last();

        comboComponent->SendInputReceived(LastInput.Key);
    }
}
