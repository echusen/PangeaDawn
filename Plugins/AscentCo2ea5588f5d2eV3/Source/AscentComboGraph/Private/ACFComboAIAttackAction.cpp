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
        UE_LOG(LogTemp, Log, TEXT("OnActionStarted: Combo graph set to AI mode"));
    }
    comboComponent = GetCharacterOwner()->FindComponentByClass<UACFComboComponent>();

    Super::OnActionStarted_Implementation();
}

void UACFComboAIAttackAction::OnActionEnded_Implementation()
{
    if (comboComponent && node) {
        if (Combo && Combo->IsAI() && comboComponent->IsExecutingAnyCombo()) {
            UE_LOG(LogTemp, Log, TEXT("OnActionEnded: Simulating AI input for combo continuation"));
            SimulateAIInput();
        } else {
            UE_LOG(LogTemp, Log, TEXT("OnActionEnded: AI combo ended or not executing."));
        }

        // Call parent implementation after simulating input
        Super::OnActionEnded_Implementation();
    } else {
        Super::OnActionEnded_Implementation(); // Always call Super for cleanup
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
