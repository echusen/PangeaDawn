#include "Tasks/MiningRoutineTask.h"
#include "Components/MiningRoutineJobComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

void UMiningRoutineTask::OnTaskStarted_Implementation(const APawn* ControlledPawn)
{
	Super::OnTaskStarted_Implementation(ControlledPawn);
	if (AController* Owner = GetOwningController())
		if (UMiningRoutineJobComponent* Job = Owner->FindComponentByClass<UMiningRoutineJobComponent>())
			Job->BeginRoutineJob(this);
}

void UMiningRoutineTask::OnTaskEnded_Implementation()
{
	if (AController* Owner = GetOwningController())
		if (UMiningRoutineJobComponent* Job = Owner->FindComponentByClass<UMiningRoutineJobComponent>())
			Job->EndRoutineJob(this);
	Super::OnTaskEnded_Implementation();
}
