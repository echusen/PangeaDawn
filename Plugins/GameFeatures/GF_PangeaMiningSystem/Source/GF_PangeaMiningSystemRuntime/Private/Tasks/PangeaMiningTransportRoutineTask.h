#pragma once
#include "CoreMinimal.h"
#include "ACFTask.h"
#include "PangeaMiningTransportRoutineTask.generated.h"

/** Instanced per controller, never placed in a shared vendor schedule. */
UCLASS()
class UPangeaMiningTransportRoutineTask final : public UACFTask
{
	GENERATED_BODY()
protected:
	virtual void OnTaskStarted_Implementation(const APawn* ControlledPawn) override;
	virtual void OnTaskEnded_Implementation() override;
};
