#include "Tasks/PangeaMiningTransportRoutineTask.h"
#include "Components/PangeaMiningTransportJobComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

void UPangeaMiningTransportRoutineTask::OnTaskStarted_Implementation(const APawn* ControlledPawn)
{
	Super::OnTaskStarted_Implementation(ControlledPawn);
	if (AController* OwningController = GetOwningController())
		if (auto* Job = OwningController->FindComponentByClass<UPangeaMiningTransportJobComponent>()) Job->BeginRoutineJob(this);
}
void UPangeaMiningTransportRoutineTask::OnTaskEnded_Implementation()
{
	if (AController* OwningController = GetOwningController())
		if (auto* Job = OwningController->FindComponentByClass<UPangeaMiningTransportJobComponent>()) Job->EndRoutineJob(this);
	Super::OnTaskEnded_Implementation();
}
