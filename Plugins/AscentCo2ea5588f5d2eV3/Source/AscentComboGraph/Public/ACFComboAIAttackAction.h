#pragma once

#include "ACFComboAttackAction.h"
#include "CoreMinimal.h"

#include "ACFComboAIAttackAction.generated.h"

// Forward Declarations
class UInputAction;
class UACFComboGraph;
class UACFComboNode;
class UACFComboComponent;

/**
 * A specialized combo attack action for AI characters.
 */
UCLASS()
class ASCENTCOMBOGRAPH_API UACFComboAIAttackAction : public UACFComboAttackAction {
    GENERATED_BODY()

public:
    UACFComboAIAttackAction();

    // AI-specific input probabilities
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ACF|AI")
    TMap<FGameplayTag, float> AIInputProbabilities;

protected:
    virtual void OnActionStarted_Implementation() override;
    virtual void OnActionEnded_Implementation() override;

private:
    void SimulateAIInput();

    TObjectPtr<UACFComboComponent> comboComponent;
};
