#pragma once

#include "CoreMinimal.h"
#include "ACFTask.h"
#include "MiningRoutineTask.generated.h"

/** Per-controller ACF schedule entry. Never store a live instance in a shared asset. */
UCLASS()
class UMiningRoutineTask final : public UACFTask
{
	GENERATED_BODY()

protected:
	virtual void OnTaskStarted_Implementation(const APawn* ControlledPawn) override;
	virtual void OnTaskEnded_Implementation() override;
};
