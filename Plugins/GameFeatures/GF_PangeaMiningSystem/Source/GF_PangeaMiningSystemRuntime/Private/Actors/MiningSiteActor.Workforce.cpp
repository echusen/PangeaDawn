#include "Actors/MiningSiteActor.h"

#include "Actors/ACFCharacter.h"
#include "Components/MiningSiteComponent.h"
#include "GameFramework/PlayerController.h"

bool AMiningSiteActor::CanRecruitFromInteraction(APawn* InteractingPawn) const
{
	const APlayerController* Player = IsValid(InteractingPawn) ? Cast<APlayerController>(InteractingPawn->GetController()) : nullptr;
	return Player && Player->GetPawn() == InteractingPawn && MiningSiteComponent &&
		FVector::DistSquared(InteractingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(600.0) &&
		MiningSiteComponent->CanRecruitWorker(ResolveUpgradeCostContext(InteractingPawn));
}

void AMiningSiteActor::HandleWorkerDeath(AACFCharacter* Worker)
{
	if (!HasAuthority() || !IsValid(Worker) || !SpawnedWorkerActors.Contains(Worker) ||
		Worker->ActorHasTag(TEXT("Mining.WorkerDeathRecorded")) || !MiningSiteComponent) return;
	Worker->Tags.AddUnique(TEXT("Mining.WorkerDeathRecorded")); // Before nested delegates/cleanup.
	MiningSiteComponent->RecordWorkerDeath();
}

void AMiningSiteActor::HandleMiningDayEnded(int64 CompletedDay, int32 LostWorkers)
{
	if (!HasAuthority() || LostWorkers <= 0) return;
	// Economic state is already committed. Use the supported ACF death path for visible actors;
	// suppress its second accounting callback. Offscreen vacancies need no spawned victim pawn.
	TArray<TObjectPtr<AActor>> Workers = SpawnedWorkerActors;
	while (!Workers.IsEmpty() && LostWorkers > 0)
	{
		// Conditional on the independent-roll loss count, victims are uniform among
		// interchangeable miners; never systematically kill the final station first.
		const int32 Index = FMath::RandHelper(Workers.Num());
		AACFCharacter* Worker = Cast<AACFCharacter>(Workers[Index]);
		Workers.RemoveAtSwap(Index);
		if (!IsValid(Worker) || !Worker->IsAlive() || Worker->ActorHasTag(TEXT("Mining.WorkerDeathRecorded"))) continue;
		Worker->Tags.AddUnique(TEXT("Mining.WorkerDeathRecorded"));
		Worker->KillCharacter();
		--LostWorkers;
	}
}

void AMiningSiteActor::HandleWorkforceChanged()
{
	RefreshPresentationActors();
	UpdatePresentationActorMovement();
	UpdateStatusText();
}
